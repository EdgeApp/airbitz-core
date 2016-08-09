/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Bitid.hpp"
#include "Login.hpp"
#include "../account/PluginData.hpp"
#include "../bitcoin/Text.hpp"
#include "../crypto/Encoding.hpp"
#include "../http/HttpRequest.hpp"
#include "../http/Uri.hpp"
#include "../json/JsonObject.hpp"
#include "../wallet/Wallet.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

static bc::hd_private_key
bitidDerivedKey(const bc::hd_private_key &root,
                const std::string &callbackUri, uint32_t index)
{
    auto hash = bc::sha256_hash(bc::build_data(
    {
        bc::to_little_endian(index), DataSlice(callbackUri)
    }));

    auto a = bc::from_little_endian<uint32_t>(hash.begin() + 0, hash.begin() + 4);
    auto b = bc::from_little_endian<uint32_t>(hash.begin() + 4, hash.begin() + 8);
    auto c = bc::from_little_endian<uint32_t>(hash.begin() + 8, hash.begin() + 12);
    auto d = bc::from_little_endian<uint32_t>(hash.begin() + 12, hash.begin() + 16);

    // TODO: Use safe HD derivation to avoid problems here.
    return root.
           generate_private_key(13 | bc::first_hardened_key).
           generate_private_key(a | bc::first_hardened_key).
           generate_private_key(b | bc::first_hardened_key).
           generate_private_key(c | bc::first_hardened_key).
           generate_private_key(d | bc::first_hardened_key);
}

Status
bitidCallback(Uri &result, const std::string &uri, bool strict)
{
    Uri out;
    if (!out.decode(uri))
        return ABC_ERROR(ABC_CC_ParseError, "Not a valid URI");
    if ("bitid" != out.scheme() || out.fragmentOk())
        return ABC_ERROR(ABC_CC_ParseError, "Not a BitID URI");
    out.authorize();
    auto u = out.queryDecode()["u"];

    // Make the adjustments:
    out.queryRemove();
    out.schemeSet("1" == u ? "http" : "https");

    result = out;
    return Status();
}

BitidSignature
bitidSign(DataSlice rootKey, const std::string &message,
          const std::string &callbackUri, uint32_t index)
{
    const auto key = bitidDerivedKey(bc::hd_private_key(rootKey),
                                     callbackUri, index);
    const auto signature = bc::sign_message(DataSlice(message),
                                            key.private_key(), true);

    BitidSignature out;
    out.address = key.address().encoded();
    out.signature = base64Encode(signature);
    return out;
}

Status
bitidLogin(DataSlice rootKey, const std::string &bitidUri, uint32_t index,
           Wallet *wallet, const std::string &kycUri)
{
    Uri callbackUri;
    ABC_CHECK(bitidCallback(callbackUri, bitidUri, false));
    const auto callback = callbackUri.encode();
    const auto domain = callbackUri.authority();

    const auto signature = bitidSign(rootKey, bitidUri, callback, index);

    struct BitidJson:
        public JsonObject
    {
        ABC_JSON_STRING(uri, "uri", nullptr);
        ABC_JSON_STRING(address, "address", nullptr);
        ABC_JSON_STRING(signature, "signature", nullptr);
        ABC_JSON_STRING(paymentAddress, "a", nullptr);
        ABC_JSON_STRING(idaddr, "idaddr", nullptr);
        ABC_JSON_STRING(idsig, "idsig", nullptr);
    };
    BitidJson json;
    ABC_CHECK(json.uriSet(bitidUri));
    ABC_CHECK(json.addressSet(signature.address));
    ABC_CHECK(json.signatureSet(signature.signature));

    // Check for extra request flags:
    ParsedUri parsedUri;
    ABC_CHECK(parseUri(parsedUri, bitidUri));

    // Attach a payment address if one is needed:
    if (parsedUri.bitidPaymentAddress && wallet)
    {
        AddressMeta address;
        wallet->addresses.getNew(address);
        ABC_CHECK(json.paymentAddressSet(address.address));

        // Set payee metadata to the domain name,
        // and finalize the address so it can't be used by others:
        address.metadata.name = domain;
        address.recyclable = false;
        wallet->addresses.save(address);
    }

    // Create a second signature signed by private key derived from specified kycUri:
    if (parsedUri.bitidKycRequest)
    {
        const auto signatureKyc = bitidSign(rootKey, bitidUri, kycUri);
        ABC_CHECK(json.idaddrSet(signatureKyc.address));
        ABC_CHECK(json.idsigSet(signatureKyc.signature));
    }

    HttpReply reply;
    HttpRequest().header("Content-Type", "application/json").
    post(reply, callback, json.encode());
    ABC_CHECK(reply.codeOk());

    // Save the domain in the account repo:
    if (parsedUri.bitidKycProvider && wallet)
    {
        ABC_CHECK(pluginDataSet(wallet->account, "Identities", domain, callback));
    }

    return Status();
}

} // namespace abcd
