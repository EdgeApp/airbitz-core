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

tABC_CC ABC_TxSetAddressRecycle(Wallet &self,
                                const char *szAddress,
                                bool bRecyclable,
                                tABC_Error *pError);

tABC_CC ABC_TxGenerateRequestQRCode(Wallet &self,
                                    const char *szRequestID,
                                    char **pszURI,
                                    unsigned char **paData,
                                    unsigned int *pWidth,
                                    tABC_Error *pError);

} // namespace abcd

#endif
