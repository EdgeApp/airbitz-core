/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Text.hpp"
#include "Testnet.hpp"
#include "../Context.hpp"
#include "../crypto/Random.hpp"
#include "../http/Uri.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

/**
 * Checks a bitcoin payment address for validity.
 */
static bool
addressOk(const std::string &text)
{
    return bc::payment_address().set_encoded(text);
}

/**
 * Checks a Casascius minikey for validity.
 */
static bool
minikeyOk(const std::string &text)
{
    // Legacy minikeys are 22 chars long
    if (text.size() != 22 && text.size() != 30)
        return false;
    return bc::sha256_hash(bc::to_data_chunk(text + "?"))[0] == 0x00;
}

/**
 * Checks an hits key for validity.
 */
static Status
hbitsOk(const std::string &text)
{
    if (text.size() != 22 && text.size() != 30)
        return ABC_ERROR(ABC_CC_ParseError, "Wrong text length");
    if (0x00 != bc::sha256_hash(bc::to_data_chunk(text + "!"))[0])
        return ABC_ERROR(ABC_CC_ParseError, "Wrong text checksum");
    return Status();
}

/**
 * Decodes an hbits private key.
 *
 * This format is very similar to the minikey format, but with some changes:
 * - The checksum character is `!` instead of `?`.
 * - The final public key is compressed.
 * - The private key must be XOR'ed with a magic constant.
 *
 * Test vector:
 * hbits://S23c2fe8dbd330539a5fbab16a7602
 * Address: 1Lbd7DZWdz7fMR1sHHnWfnfQeAFoT52ZAi
 */
static Status
hbitsDecode(bc::ec_secret &result, const std::string &text)
{
    ABC_CHECK(hbitsOk(text));

    // Extract the secret:
    result = bc::sha256_hash(bc::to_data_chunk(text));

    // XOR with our magic number:
    auto mix = bc::decode_hex(gContext->hiddenBitsKey());
    for (size_t i = 0; i < mix.size() && i < result.size(); ++i)
        result[i] ^= mix[i];

    return Status();
}

Status
parseUri(ParsedUri &result, const std::string &text)
{
    Uri uri;

    if (uri.decode(text))
    {
        // Turn Airbitz URI's into bitcoin URI's:
        if ("airbitz" == uri.scheme())
        {
            uri.deauthorize();
            auto path = uri.path();
            if (0 != path.find("bitcoin/", 0))
                return ABC_ERROR(ABC_CC_ParseError, "Unknown airbitz URI");
            path.erase(0, 8);
            uri.pathSet(path);
            uri.schemeSet("bitcoin");
        }

        // Check the scheme:
        if ("bitcoin" == uri.scheme())
        {
            uri.deauthorize();
            if (addressOk(uri.path()))
                result.address = uri.path();

            auto query = uri.queryDecode();
            bc::decode_base10(result.amountSatoshi, query["amount"], 8);
            result.label = query["label"];
            result.label = query["message"];
            result.label = query["category"];
            result.label = query["ret"];
            result.paymentProto = query["r"];
        }
        else if ("hbits" == uri.scheme())
        {
            uri.deauthorize();
            bc::ec_secret secret;
            ABC_CHECK(hbitsDecode(secret, uri.path()));
            result.wif = bc::secret_to_wif(secret, true);
        }
        else if ("bitid" == uri.scheme())
        {
            result.bitidUri = text;
        }
        else
        {
            return ABC_ERROR(ABC_CC_ParseError, "Unknown URI scheme");
        }
    }
    else if (addressOk(text))
    {
        // This is a raw bitcoin address:
        result.address = text;
    }
    else if (bc::null_hash != bc::wif_to_secret(text))
    {
        // This is a raw WIF private key:
        result.wif = text;
    }
    else if (minikeyOk(text))
    {
        // This is a raw Casascius minikey:
        result.wif = bc::secret_to_wif(bc::minikey_to_secret(text), false);
    }
    else if (hbitsOk(text))
    {
        // This is a raw hbits key:
        bc::ec_secret secret;
        ABC_CHECK(hbitsDecode(secret, text));
        result.wif = bc::secret_to_wif(secret, true);
    }
    else
    {
        return ABC_ERROR(ABC_CC_ParseError, "Malformed bitcoin URI");
    }

    // Private keys also have addresses:
    if (result.wif.size())
    {
        const auto compressed =  bc::is_wif_compressed(result.wif);
        const auto secret = bc::wif_to_secret(result.wif);
        const auto pubkey = bc::secret_to_public_key(secret, compressed);
        bc::payment_address address;
        address.set(pubkeyVersion(), bc::bitcoin_short_hash(pubkey));
        result.address = address.encoded();
    }

    return Status();
}

Status
hbitsCreate(std::string &result, std::string &addressOut)
{
    while (true)
    {
        libbitcoin::data_chunk cand(21);
        ABC_CHECK(randomData(cand, cand.size()));
        std::string minikey = libbitcoin::encode_base58(cand);
        minikey.insert(0, "a");
        if (30 == minikey.size() &&
                0x00 == bc::sha256_hash(bc::to_data_chunk(minikey + "!"))[0])
        {
            bc::ec_secret secret;
            hbitsDecode(secret, minikey);
            bc::ec_point pubkey = bc::secret_to_public_key(secret, true);
            bc::payment_address address;
            address.set(pubkeyVersion(), bc::bitcoin_short_hash(pubkey));

            result = minikey;
            addressOut = address.encoded();
            return Status();
        }
    }
}

} // namespace abcd
