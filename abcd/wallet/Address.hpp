/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_WALLET_ADDRESS_HPP
#define ABCD_WALLET_ADDRESS_HPP

#include "../util/Status.hpp"

namespace abcd {

struct TxMetadata;
class Wallet;

tABC_CC ABC_TxWatchAddresses(Wallet &self,
                             tABC_Error *pError);

tABC_CC ABC_TxCreateReceiveRequest(Wallet &self,
                                   const TxMetadata &metadata,
                                   char **pszRequestID,
                                   bool bTransfer,
                                   tABC_Error *pError);

tABC_CC ABC_TxModifyReceiveRequest(Wallet &self,
                                   const char *szRequestID,
                                   const TxMetadata &metadata,
                                   tABC_Error *pError);

tABC_CC ABC_TxFinalizeReceiveRequest(Wallet &self,
                                     const char *szRequestID,
                                     tABC_Error *pError);

tABC_CC ABC_TxCancelReceiveRequest(Wallet &self,
                                   const char *szRequestID,
                                   tABC_Error *pError);

tABC_CC ABC_TxGenerateRequestQRCode(Wallet &self,
                                    const char *szRequestID,
                                    char **pszURI,
                                    unsigned char **paData,
                                    unsigned int *pWidth,
                                    tABC_Error *pError);

tABC_CC ABC_TxGetRequestAddress(Wallet &self,
                                const char *szRequestID,
                                char **pszAddress,
                                tABC_Error *pError);

} // namespace abcd

#endif
