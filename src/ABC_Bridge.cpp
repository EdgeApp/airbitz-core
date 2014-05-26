/**
 * @file
 * AirBitz C++ wrappers.
 *
 * This file contains a bridge between the plain C code in the core, and
 * the C++ code in libbitcoin and friends.
 *
 * @author William Swanson
 * @version 0.1
 */

#include "ABC_Bridge.h"
#include <wallet/uri.hpp>
#include <wallet/hd_keys.hpp>
#include <bitcoin/utility/base58.hpp>

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
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_BitcoinURIInfo *pInfo = NULL;

    ABC_CHECK_NULL(szURI);
    ABC_CHECK_ASSERT(strlen(szURI) > 0, ABC_CC_Error, "No URI provided");
    ABC_CHECK_NULL(ppInfo);
    *ppInfo = NULL;

    // allocate initial struct
    ABC_ALLOC_ARRAY(pInfo, 1, tABC_BitcoinURIInfo);

    // parse and extract contents
    if (!libwallet::uri_parse(szURI, result))
        ABC_RET_ERROR(ABC_CC_ParseError, "Malformed bitcoin URI");
    if (result.address)
        ABC_STRDUP(pInfo->szAddress, result.address.get().encoded().c_str());
    if (result.amount)
        pInfo->amountSatoshi = result.amount.get();
    if (result.label)
        ABC_STRDUP(pInfo->szLabel, result.label.get().c_str());
    if (result.message)
        ABC_STRDUP(pInfo->szMessage, result.message.get().c_str());

    // assign created info struct
    *ppInfo = pInfo;
    pInfo = NULL; // do this so we don't free below what we just gave the caller

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
                              int64_t *pAmountOut,
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
 */
tABC_CC ABC_BridgeFormatAmount(int64_t amount,
                               char **pszAmountOut,
                               unsigned decimalPlaces,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::string out;

    ABC_CHECK_NULL(pszAmountOut);

    out = libwallet::format_amount(amount, decimalPlaces);
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

/**
 * Converts a Base58-encoded string to a block of data.
 *
 * @param szBase58 The string to convert.
 * @param pData A buffer structure that will be pointed at the newly-allocated output data.
 */
tABC_CC ABC_BridgeBase58Decode(const char *szBase58, tABC_U08Buf *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    libbitcoin::data_chunk out;

    std::string in = szBase58;
    if (!libbitcoin::is_base58(in))
        ABC_RET_ERROR(ABC_CC_ParseError, "Not Base58 data");

    out = libbitcoin::decode_base58(in);

    pData->p = static_cast<unsigned char*>(malloc(out.size()));
    ABC_CHECK_ASSERT(pData->p != NULL, ABC_CC_NULLPtr, "malloc failed (returned NULL)");
    pData->end = pData->p + out.size();
    memcpy(pData->p, out.data(), out.size());

exit:
    return cc;
}

/**
 * Calculates a public address for the HD wallet main external chain.
 * @param pszPubAddress set to the newly-generated address, or set to NULL if
 * there is a math error. If that happens, add 1 to N and try again.
 * @param PrivateSeed any amount of random data to seed the generator
 * @param N the index of the key to generate
 */
tABC_CC ABC_BridgeGetBitcoinPubAddress(char **pszPubAddress,
                                       tABC_U08Buf PrivateSeed,
                                       int32_t N,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    libbitcoin::data_chunk seed(PrivateSeed.p, PrivateSeed.end);
    libwallet::hd_private_key m(seed);
    libwallet::hd_private_key m0 = m.generate_private_key(0);
    libwallet::hd_private_key m00 = m0.generate_private_key(0);
    libwallet::hd_private_key m00n = m00.generate_private_key(N);
    if (m00n.valid())
    {
        std::string out = m00n.address().encoded();
        ABC_STRDUP(*pszPubAddress, out.c_str());
    }
    else
    {
        *pszPubAddress = nullptr;
    }

exit:
    return cc;
}
