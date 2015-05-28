/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_SPEND_SPEND_HPP
#define ABCD_SPEND_SPEND_HPP

#include "../Wallet.hpp"
#include "../util/Status.hpp"

namespace abcd {

/**
 * AirBitz Core Send Tx Structure
 *
 * This structure contains the detailed information associated
 * with initiating a send.
 */
typedef struct sABC_TxSendInfo
{
    char                    *szDestAddress;
    tABC_TxDetails          *pDetails;

    // Transfer from money from one wallet to another
    bool                    bTransfer;
    tABC_WalletID           walletDest;

} tABC_TxSendInfo;

void ABC_TxSendInfoFree(tABC_TxSendInfo *pTxSendInfo);

tABC_CC ABC_TxSendInfoAlloc(tABC_TxSendInfo **ppTxSendInfo,
                            const char *szDestAddress,
                            const tABC_TxDetails *pDetails,
                            tABC_Error *pError);

tABC_CC  ABC_TxCalcSendFees(tABC_WalletID self,
                            tABC_TxSendInfo *pInfo,
                            uint64_t *pTotalFees,
                            tABC_Error *pError);

tABC_CC  ABC_TxSend(tABC_WalletID self,
                    tABC_TxSendInfo *pInfo,
                    char **pszTxID,
                    tABC_Error *pError);

} // namespace abcd

#endif
