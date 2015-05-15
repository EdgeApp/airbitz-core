/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Helpers for dealing with Bitcoin-related text formats.
 */

#ifndef ABCD_BITCOIN_TEXT_HPP
#define ABCD_BITCOIN_TEXT_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"

namespace abcd {

/**
 * AirBitz Bitcoin URI Elements
 */
typedef struct sABC_BitcoinURIInfo
{
    /** label for that address (e.g. name of receiver) */
    char *szLabel;
    /** bitcoin address (base58) */
    char *szAddress;
    /** message that shown to the user after scanning the QR code */
    char *szMessage;
    /** amount of bitcoins */
    int64_t amountSatoshi;
    /** Airbitz category extension */
    char *szCategory;
    /** Airbitz ret extension for return URI */
    char *szRet;
} tABC_BitcoinURIInfo;

tABC_CC ABC_BridgeDecodeWIF(const char *szWIF,
                            tABC_U08Buf *pOut,
                            bool *pbCompressed,
                            char **pszAddress,
                            tABC_Error *pError);

tABC_CC ABC_BridgeParseBitcoinURI(std::string uri,
                                  tABC_BitcoinURIInfo **ppInfo,
                                  tABC_Error *pError);

void ABC_BridgeFreeURIInfo(tABC_BitcoinURIInfo *pInfo);

tABC_CC ABC_BridgeParseAmount(const char *szAmount,
                              uint64_t *pAmountOut,
                              unsigned decimalPlaces);

tABC_CC ABC_BridgeFormatAmount(int64_t amount,
                               char **pszAmountOut,
                               unsigned decimalPlaces,
                               bool bAddSign,
                               tABC_Error *pError);

tABC_CC ABC_BridgeEncodeBitcoinURI(char **pszURI,
                                   tABC_BitcoinURIInfo *pInfo,
                                   tABC_Error *pError);

} // namespace abcd

#endif
