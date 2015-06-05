/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_SPEND_SPEND_HPP
#define ABCD_SPEND_SPEND_HPP

#include "../Wallet.hpp"
#include "../util/Status.hpp"

namespace abcd {

class PaymentRequest;

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
    tABC_WalletID           walletDest;

};

tABC_CC  ABC_TxCalcSendFees(tABC_WalletID self,
                            SendInfo *pInfo,
                            uint64_t *pTotalFees,
                            tABC_Error *pError);

tABC_CC ABC_BridgeMaxSpendable(tABC_WalletID self,
                               SendInfo *pInfo,
                               uint64_t *pMaxSatoshi,
                               tABC_Error *pError);

tABC_CC  ABC_TxSend(tABC_WalletID self,
                    SendInfo *pInfo,
                    char **pszTxID,
                    tABC_Error *pError);

} // namespace abcd

#endif
