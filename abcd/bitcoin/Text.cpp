/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Text.hpp"
#include "Testnet.hpp"
#include "../config.h"
#include <wallet/wallet.hpp>

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
    auto mix = bc::decode_hex(HIDDENBITZ_KEY);
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
    secret = libwallet::wif_to_secret(wif);
    if (secret != bc::null_hash)
    {
        bCompressed = libwallet::is_wif_compressed(wif);
    }
    else if (minikeyCheck(wif))
    {
        secret = libwallet::minikey_to_secret(wif);
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
    ABC_STRDUP(szAddress, address.encoded().c_str());

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
    public libwallet::uri_parse_result
{
    optional_string category;
    optional_string ret;

protected:
    virtual bool got_param(std::string& key, std::string& value)
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

    tABC_BitcoinURIInfo *pInfo = NULL;
    CustomResult result;

    // Allow a double-slash in the "bitcoin:" URI schema
    // to work around limitations in email and SMS programs:
    if (0 == uri.find("bitcoin://", 0))
        uri.replace(0, 10, "bitcoin:");

    // Handle our own URI schema to allow apps to target us specifically:
    if (0 == uri.find("airbitz://bitcoin/", 0))
        uri.replace(0, 18, "bitcoin:");

    // Try to parse as a URI:
    if (!libwallet::uri_parse(uri, result))
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
    ABC_NEW(pInfo, tABC_BitcoinURIInfo);
    if (result.address)
    {
        auto address = result.address.get();
        if (address.version() == pubkeyVersion() ||
            address.version() == scriptVersion())
            ABC_STRDUP(pInfo->szAddress, address.encoded().c_str());
    }
    if (result.amount)
        pInfo->amountSatoshi = result.amount.get();
    if (result.label)
        ABC_STRDUP(pInfo->szLabel, result.label.get().c_str());
    if (result.message)
        ABC_STRDUP(pInfo->szMessage, result.message.get().c_str());
    if (result.category)
    {
        auto category = result.category.get();
        if (0 == category.find("Expense:") ||
            0 == category.find("Income:") ||
            0 == category.find("Transfer:") ||
            0 == category.find("Exchange:"))
            ABC_STRDUP(pInfo->szCategory, category.c_str());
    }
    if (result.r)
        ABC_STRDUP(pInfo->szR, result.r.get().c_str());
    if (result.ret)
        ABC_STRDUP(pInfo->szRet, result.ret.get().c_str());

    // assign created info struct
    *ppInfo = pInfo;
    pInfo = nullptr;

exit:
    ABC_BridgeFreeURIInfo(pInfo);

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
 * Parses a Bitcoin amount string to an integer.
 * @param the amount to parse, in bitcoins
 * @param the integer value, in satoshis, or ABC_INVALID_AMOUNT
 * if something goes wrong.
 * @param decimal_places set to ABC_BITCOIN_DECIMAL_PLACE to convert
 * bitcoin to satoshis.
 */
tABC_CC ABC_BridgeParseAmount(const char *szAmount,
                              uint64_t *pAmountOut,
                              unsigned decimalPlaces)
{
    *pAmountOut = libwallet::parse_amount(szAmount, decimalPlaces);
    return ABC_CC_Ok;
}

/**
 * Formats a Bitcoin integer amount as a string, avoiding the rounding
 * problems typical with floating-point math.
 * @param amount the number of satoshis
 * @param pszAmountOut a pointer that will hold the output string. The
 * caller frees the returned value.
 * @param decimal_places set to ABC_BITCOIN_DECIMAL_PLACE to convert
 * satoshis to bitcoins.
 * @param bAddSign set to 'true' to add negative symbol to string if
 * amount is negative
 */
tABC_CC ABC_BridgeFormatAmount(int64_t amount,
                               char **pszAmountOut,
                               unsigned decimalPlaces,
                               bool bAddSign,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::string out;

    ABC_CHECK_NULL(pszAmountOut);

    if (amount < 0)
    {
        out = libwallet::format_amount(-amount, decimalPlaces);
        if (bAddSign)
            out.insert(0, 1, '-');
    }
    else
    {
        out = libwallet::format_amount(amount, decimalPlaces);
    }
    ABC_STRDUP(*pszAmountOut, out.c_str());

exit:
    return cc;
}

/**
 *
 */
tABC_CC ABC_BridgeEncodeBitcoinURI(char **pszURI,
                                   tABC_BitcoinURIInfo *pInfo,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    libwallet::uri_writer writer;
    if (pInfo->szAddress)
        writer.write_address(pInfo->szAddress);
    if (pInfo->amountSatoshi)
        writer.write_amount(pInfo->amountSatoshi);
    if (pInfo->szLabel)
        writer.write_param("label", pInfo->szLabel);
    if (pInfo->szMessage)
        writer.write_param("message", pInfo->szMessage);

    ABC_STRDUP(*pszURI, writer.string().c_str());

exit:
    return cc;
}

} // namespace abcd
