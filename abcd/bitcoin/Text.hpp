/*
 * Copyright (c) 2014, Airbitz, Inc.
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
    const char *szLabel;
    /** bitcoin address (base58) */
    const char *szAddress;
    /** message that shown to the user after scanning the QR code */
    const char *szMessage;
    /** amount of bitcoins */
    int64_t amountSatoshi;
    /** Bip72 payment request r parameter */
    const char *szR;
    /** Airbitz category extension */
    const char *szCategory;
    /** Airbitz ret extension for return URI */
    const char *szRet;
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

/**
 * Generate a random hbits private key.
 */
Status
hbitsCreate(std::string &result, std::string &addressOut);

} // namespace abcd

#endif
