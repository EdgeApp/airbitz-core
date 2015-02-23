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

#define MAX_BTC_STRING_SIZE 20

/**
 * Attempts to find the bitcoin address for a private key.
 * @return false for failure.
 */
static
bool ABC_BridgeDecodeWIFAddress(bc::payment_address& address, std::string wif)
{
    tABC_Error error;
    AutoU08Buf secret;
    bool bCompressed;
    char *szAddress = NULL;

    // If the text starts with "hbitz://", get rid of that:
    if (0 == wif.find("hbits://"))
        wif.erase(0, 8);

    // Try to parse this as a key:
    if (ABC_CC_Ok != ABC_BridgeDecodeWIF(wif.c_str(),
        &secret, &bCompressed, &szAddress, &error))
        return false;

    // Output:
    if (!address.set_encoded(szAddress))
    {
        ABC_FREE_STR(szAddress);
        return false;
    }
    ABC_FREE_STR(szAddress);
    return true;
}

bool check_minikey(const std::string& minikey)
{
    // Legacy minikeys are 22 chars long
    if (minikey.size() != 22 && minikey.size() != 30)
        return false;
    return bc::sha256_hash(bc::to_data_chunk(minikey + "?"))[0] == 0x00;
}

bool check_hiddenbitz(const std::string& minikey)
{
    // Legacy minikeys are 22 chars long
    if (minikey.size() != 22 && minikey.size() != 30)
        return false;
    return bc::sha256_hash(bc::to_data_chunk(minikey + "!"))[0] == 0x00;
}

bc::ec_secret hiddenbitz_to_secret(const std::string& minikey)
{
    if (!check_hiddenbitz(minikey))
        return bc::ec_secret();
    auto secret = bc::sha256_hash(bc::to_data_chunk(minikey));
    auto mix = bc::decode_hex(HIDDENBITZ_KEY);

    for (size_t i = 0; i < mix.size() && i < secret.size(); ++i)
        secret[i] ^= mix[i];

    return secret;
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

    // Parse as WIF:
    secret = libwallet::wif_to_secret(szWIF);
    if (secret != bc::null_hash)
    {
        bCompressed = libwallet::is_wif_compressed(szWIF);
    }
    else if (check_minikey(szWIF))
    {
        secret = libwallet::minikey_to_secret(szWIF);
        bCompressed = false;
    }
    else if (check_hiddenbitz(szWIF))
    {
        secret = hiddenbitz_to_secret(szWIF);
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
    ABC_BUF_DUP_PTR(*pOut, secret.data(), secret.size());
    *pbCompressed = bCompressed;
    *pszAddress = szAddress;
    szAddress = NULL;

exit:
    ABC_FREE_STR(szAddress);

    return cc;
}

/**
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param szURI     URI to parse
 * @param ppInfo    Pointer to location to store allocated info struct.
 *                  If a member is not present in the URI, the corresponding
 *                  string poiner in the struct will be NULL.
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_BridgeParseBitcoinURI(const char *szURI,
                            tABC_BitcoinURIInfo **ppInfo,
                            tABC_Error *pError)
{
    libwallet::uri_parse_result result;
    bc::payment_address address;
    char *uriString = NULL;
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_BitcoinURIInfo *pInfo = NULL;
    std::string tempStr = szURI;

    ABC_CHECK_NULL(szURI);
    ABC_CHECK_ASSERT(strlen(szURI) > 0, ABC_CC_Error, "No URI provided");
    ABC_CHECK_NULL(ppInfo);
    *ppInfo = NULL;

    // allocate initial struct
    ABC_NEW(pInfo, tABC_BitcoinURIInfo);

    // XX semi-hack warning. Might not be BIP friendly. Convert "bitcoin://1zf7ef..." URIs to
    // "bitcoin:1zf7ef..." so that libwallet parser doesn't choke. "bitcoin://" URLs are the
    // only style that are understood by email and SMS readers and will get forwarded
    // to bitcoin wallets. Airbitz wallet creates "bitcoin://" URIs when requesting funds via
    // email/SMS so it should be able to parse them as well. -paulvp

    ABC_STRDUP(uriString, szURI);

    if (0 == tempStr.find("bitcoin://", 0))
    {
        tempStr.replace(0, 10, "bitcoin:");
        std::size_t length = tempStr.copy(uriString,0xFFFFFFFF);
        uriString[length] = '\0';
    }

    // Try to parse as a URI:
    if (!libwallet::uri_parse(uriString, result))
    {
        // Try to parse as a raw address:
        if (!address.set_encoded(uriString))
        {
            // Try to parse as a private key:
            if (!ABC_BridgeDecodeWIFAddress(address, uriString))
            {
                ABC_RET_ERROR(ABC_CC_ParseError, "Malformed bitcoin URI");
            }
        }
        result.address = address;
    }
    if (result.address)
        ABC_STRDUP(pInfo->szAddress, result.address.get().encoded().c_str());
    if (result.amount)
        pInfo->amountSatoshi = result.amount.get();
    if (result.label)
        ABC_STRDUP(pInfo->szLabel, result.label.get().c_str());
    if (result.message)
        ABC_STRDUP(pInfo->szMessage, result.message.get().c_str());

    // Reject altcoin addresses:
    if (result.address.get().version() != pubkeyVersion() &&
        result.address.get().version() != scriptVersion())
    {
        ABC_RET_ERROR(ABC_CC_ParseError, "Wrong network URI");
    }

    // assign created info struct
    *ppInfo = pInfo;
    pInfo = NULL; // do this so we don't free below what we just gave the caller

exit:
    ABC_BridgeFreeURIInfo(pInfo);
    ABC_FREE(uriString);

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
    char *szBuff;
    std::string out;

    ABC_CHECK_NULL(pszAmountOut);

    if (bAddSign && (amount < 0))
    {
        amount = llabs(amount);
        out = libwallet::format_amount(amount, decimalPlaces);
        ABC_STR_NEW(szBuff, MAX_BTC_STRING_SIZE);
        snprintf(szBuff, MAX_BTC_STRING_SIZE, "-%s", out.c_str());
        *pszAmountOut = szBuff;

    }
    else
    {
        amount = llabs(amount);
        out = libwallet::format_amount((uint64_t) amount, decimalPlaces);
        ABC_STRDUP(*pszAmountOut, out.c_str());
    }

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

/**
 * Converts a block of data to a Base58-encoded string.
 *
 * @param Data Buffer of data to convert.
 * @param pszBase58 Output string, allocated by this function.
 */
tABC_CC ABC_BridgeBase58Encode(tABC_U08Buf Data, char **pszBase58, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    libbitcoin::data_chunk in(Data.p, Data.end);
    std::string out = libbitcoin::encode_base58(in);

    ABC_STRDUP(*pszBase58, out.c_str());

exit:
    return cc;
}

} // namespace abcd
