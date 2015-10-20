/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_SPEND_SPEND_HPP
#define ABCD_SPEND_SPEND_HPP

#include "../util/Status.hpp"

namespace abcd {

class PaymentRequest;
class Wallet;

/**
 * Options for sending money.
 */
struct SendInfo
{
    ~SendInfo();
    SendInfo();

    char                    *szDestAddress;
    PaymentRequest          *paymentRequest;
    tABC_TxDetails          *pDetails;

    // Transfer from one wallet to another:
    bool                    bTransfer;
    Wallet                  *walletDest;

};

tABC_CC  ABC_TxCalcSendFees(Wallet &self,
                            SendInfo *pInfo,
                            uint64_t *pTotalFees,
                            tABC_Error *pError);

tABC_CC ABC_BridgeMaxSpendable(Wallet &self,
                               SendInfo *pInfo,
                               uint64_t *pMaxSatoshi,
                               tABC_Error *pError);

tABC_CC  ABC_TxSend(Wallet &self,
                    SendInfo *pInfo,
                    char **pszNtxid,
                    tABC_Error *pError);

} // namespace abcd

#endif
