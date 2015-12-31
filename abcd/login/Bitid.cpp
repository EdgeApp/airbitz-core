/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Bitid.hpp"
#include "Login.hpp"
#include "../crypto/Encoding.hpp"
#include "../http/HttpRequest.hpp"
#include "../http/Uri.hpp"
#include "../json/JsonObject.hpp"
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
bitidLogin(DataSlice rootKey, const std::string &bitidUri, uint32_t index)
{
    Uri callbackUri;
    ABC_CHECK(bitidCallback(callbackUri, bitidUri, false));
    auto callback = callbackUri.encode();

    const auto signature = bitidSign(rootKey, bitidUri, callback, index);

    struct BitidJson:
        public JsonObject
    {
        ABC_JSON_STRING(uri, "uri", nullptr);
        ABC_JSON_STRING(address, "address", nullptr);
        ABC_JSON_STRING(signature, "signature", nullptr);
    };
    BitidJson json;
    ABC_CHECK(json.uriSet(bitidUri));
    ABC_CHECK(json.addressSet(signature.address));
    ABC_CHECK(json.signatureSet(signature.signature));

    HttpReply reply;
    HttpRequest().header("Content-Type", "application/json").
    post(reply, callback, json.encode());
    ABC_CHECK(reply.codeOk());

    return Status();
}

} // namespace abcd
