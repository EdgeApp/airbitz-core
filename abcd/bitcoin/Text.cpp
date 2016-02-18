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
#include "../util/Util.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

/**
 * Attempts to find the bitcoin address for a private key.
 * @return false for failure.
 */
static Status
wifToAddress(bc::payment_address &result, const std::string &wif)
{
    AutoU08Buf secret;
    AutoString szAddress;
    bool bCompressed;
    ABC_CHECK_OLD(ABC_BridgeDecodeWIF(wif.c_str(),
                                      &secret, &bCompressed, &szAddress.get(), &error));

    // Output:
    if (!result.set_encoded(szAddress.get()))
        return ABC_ERROR(ABC_CC_ParseError, "Not a private key");

    return Status();
}

static bool
minikeyCheck(const std::string &minikey)
{
    // Legacy minikeys are 22 chars long
    if (minikey.size() != 22 && minikey.size() != 30)
        return false;
    return bc::sha256_hash(bc::to_data_chunk(minikey + "?"))[0] == 0x00;
}

/**
 * Decodes an `hbits` private key.
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
hbitsDecode(bc::ec_secret &result, const std::string &hbits)
{
    // If the text starts with "hbitz://", get rid of that:
    std::string trimmed = hbits;
    if (0 == trimmed.find("hbits://"))
        trimmed.erase(0, 8);

    // Sanity checks:
    if (trimmed.size() != 22 && trimmed.size() != 30)
        return ABC_ERROR(ABC_CC_ParseError, "Wrong hbits length");
    if (0x00 != bc::sha256_hash(bc::to_data_chunk(trimmed + "!"))[0])
        return ABC_ERROR(ABC_CC_ParseError, "Wrong hbits checksum");

    // Extract the secret:
    result = bc::sha256_hash(bc::to_data_chunk(trimmed));

    // XOR with our magic number:
    auto mix = bc::decode_hex(gContext->hiddenBitsKey());
    for (size_t i = 0; i < mix.size() && i < result.size(); ++i)
        result[i] ^= mix[i];

    return Status();
}

/**
 * Converts a Bitcoin private key in WIF format into a 256-bit value.
 */
tABC_CC ABC_BridgeDecodeWIF(const char *szWIF,
                            tABC_U08Buf *pOut,
                            bool *pbCompressed,
                            char **pszAddress,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    bc::ec_secret secret;
    bc::data_chunk ec_addr;
    bc::payment_address address;

    bool bCompressed = true;
    char *szAddress = NULL;

    std::string wif = szWIF;

    // Parse as WIF:
    secret = bc::wif_to_secret(wif);
    if (secret != bc::null_hash)
    {
        bCompressed = bc::is_wif_compressed(wif);
    }
    else if (minikeyCheck(wif))
    {
        secret = bc::minikey_to_secret(wif);
        bCompressed = false;
    }
    else if (hbitsDecode(secret, wif))
    {
        bCompressed = true;
    }
    else
    {
        ABC_RET_ERROR(ABC_CC_ParseError, "Malformed WIF");
    }

    // Get address:
    ec_addr = bc::secret_to_public_key(secret, bCompressed);
    address.set(pubkeyVersion(), bc::bitcoin_short_hash(ec_addr));
    szAddress = stringCopy(address.encoded());

    // Write out:
    ABC_BUF_DUP(*pOut, U08Buf(secret.data(), secret.size()));
    *pbCompressed = bCompressed;
    *pszAddress = szAddress;
    szAddress = NULL;

exit:
    ABC_FREE_STR(szAddress);

    return cc;
}

struct CustomResult:
    public bc::uri_parse_result
{
    optional_string category;
    optional_string ret;

protected:
    virtual bool got_param(std::string &key, std::string &value)
    {
        if ("category" == key)
            category.reset(value);
        if ("ret" == key)
            ret.reset(value);
        return uri_parse_result::got_param(key, value);
    }
};

/**
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param ppInfo    Pointer to location to store allocated info struct.
 *                  If a member is not present in the URI, the corresponding
 *                  string poiner in the struct will be NULL.
 */
tABC_CC ABC_BridgeParseBitcoinURI(std::string uri,
                                  tABC_BitcoinURIInfo **ppInfo,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    CustomResult result;
    AutoFree<tABC_BitcoinURIInfo, ABC_BridgeFreeURIInfo>
    pInfo(structAlloc<tABC_BitcoinURIInfo>());

    // Allow a double-slash in the "bitcoin:" URI schema
    // to work around limitations in email and SMS programs:
    if (0 == uri.find("bitcoin://", 0))
        uri.replace(0, 10, "bitcoin:");

    // Handle our own URI schema to allow apps to target us specifically:
    if (0 == uri.find("airbitz://bitcoin/", 0))
        uri.replace(0, 18, "bitcoin:");

    // Try to parse as a URI:
    if (!bc::uri_parse(uri, result, false))
    {
        // Try to parse as a raw address:
        bc::payment_address address;
        if (!address.set_encoded(uri))
        {
            // Try to parse as a private key:
            if (!wifToAddress(address, uri))
            {
                ABC_RET_ERROR(ABC_CC_ParseError, "Malformed bitcoin URI");
            }
        }
        result.address = address;
    }

    // Copy into the output struct:
    if (result.address)
    {
        auto address = result.address.get();
        if (address.version() == pubkeyVersion() ||
                address.version() == scriptVersion())
            pInfo->szAddress = stringCopy(address.encoded());
    }
    if (result.amount)
        pInfo->amountSatoshi = result.amount.get();
    if (result.label)
        pInfo->szLabel = stringCopy(result.label.get());
    if (result.message)
        pInfo->szMessage = stringCopy(result.message.get());
    if (result.category)
    {
        auto category = result.category.get();
        if (0 == category.find("Expense:") ||
                0 == category.find("Income:") ||
                0 == category.find("Transfer:") ||
                0 == category.find("Exchange:"))
            pInfo->szCategory = stringCopy(category);
    }
    if (result.r)
        pInfo->szR = stringCopy(result.r.get());
    if (result.ret)
        pInfo->szRet = stringCopy(result.ret.get());

    // assign created info struct
    *ppInfo = pInfo.release();

exit:
    return cc;
}

/**
 * Frees the memory allocated by ABC_BridgeParseBitcoinURI.
 *
 * @param pInfo Pointer to allocated info struct.
 */
void ABC_BridgeFreeURIInfo(tABC_BitcoinURIInfo *pInfo)
{
    if (pInfo != NULL)
    {
        ABC_FREE_STR(pInfo->szLabel);
        ABC_FREE_STR(pInfo->szAddress);
        ABC_FREE_STR(pInfo->szMessage);
        ABC_FREE_STR(pInfo->szR);
        ABC_FREE_STR(pInfo->szCategory);
        ABC_FREE_STR(pInfo->szRet);
        ABC_CLEAR_FREE(pInfo, sizeof(tABC_BitcoinURIInfo));
    }
}

/**
 *
 */
tABC_CC ABC_BridgeEncodeBitcoinURI(char **pszURI,
                                   tABC_BitcoinURIInfo *pInfo,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    bc::uri_writer writer;
    if (pInfo->szAddress)
        writer.write_address(pInfo->szAddress);
    if (pInfo->amountSatoshi)
        writer.write_amount(pInfo->amountSatoshi);
    if (pInfo->szLabel)
        writer.write_param("label", pInfo->szLabel);
    if (pInfo->szMessage)
        writer.write_param("message", pInfo->szMessage);

    *pszURI = stringCopy(writer.string());

    return cc;
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
