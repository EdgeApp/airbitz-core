/*
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 */
/**
 * @file
 * Functions for creating, viewing, and modifying transaction meta-data.
 */

#ifndef ABC_Tx_h
#define ABC_Tx_h

#include "../src/ABC.h"
#include "Wallet.hpp"
#include "util/Sync.hpp"

namespace abcd {

/**
 * AirBitz Core Send Tx Structure
 *
 * This structure contains the detailed information associated
 * with initiating a send.
 *
 */
typedef struct sABC_TxSendInfo
{
    tABC_WalletID           wallet;
    char                    *szDestAddress;

    // Transfer from money from one wallet to another
    bool                    bTransfer;
    char                    *szDestWalletUUID;
    char                    *szDestName;
    char                    *szDestCategory;
    char                    *szSrcName;
    char                    *szSrcCategory;

    tABC_TxDetails          *pDetails;

    /** information the error if there was a failure */
    tABC_Error  errorInfo;
} tABC_TxSendInfo;


tABC_CC ABC_TxInitialize(tABC_Error *pError);

tABC_CC ABC_TxDupDetails(tABC_TxDetails **ppNewDetails,
                         const tABC_TxDetails *pOldDetails,
                         tABC_Error *pError);

void ABC_TxFreeDetails(tABC_TxDetails *pDetails);
void ABC_TxFreeOutput(tABC_TxOutput *pOutputs);
void ABC_TxFreeOutputs(tABC_TxOutput **aOutputs, unsigned int count);

tABC_CC ABC_TxSendInfoAlloc(tABC_TxSendInfo **ppTxSendInfo,
                            tABC_WalletID self,
                            const char *szDestAddress,
                            const tABC_TxDetails *pDetails,
                            tABC_Error *pError);


void ABC_TxSendInfoFree(tABC_TxSendInfo *pTxSendInfo);

double ABC_TxSatoshiToBitcoin(int64_t satoshi);

int64_t ABC_TxBitcoinToSatoshi(double bitcoin);

tABC_CC ABC_TxBlockHeightUpdate(uint64_t height,
                                tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                                void *pData,
                                tABC_Error *pError);

tABC_CC ABC_TxReceiveTransaction(tABC_WalletID self,
                                 uint64_t amountSatoshi, uint64_t feeSatoshi,
                                 tABC_TxOutput **paInAddress, unsigned int inAddressCount,
                                 tABC_TxOutput **paOutAddresses, unsigned int outAddressCount,
                                 const char *szTxId, const char *szMalTxId,
                                 tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                                 void *pData,
                                 tABC_Error *pError);

tABC_CC ABC_TxCreateInitialAddresses(tABC_WalletID self,
                                     tABC_Error *pError);

tABC_CC ABC_TxCreateReceiveRequest(tABC_WalletID self,
                                   tABC_TxDetails *pDetails,
                                   char **pszRequestID,
                                   bool bTransfer,
                                   tABC_Error *pError);

tABC_CC ABC_TxModifyReceiveRequest(tABC_WalletID self,
                                   const char *szRequestID,
                                   tABC_TxDetails *pDetails,
                                   tABC_Error *pError);

tABC_CC ABC_TxFinalizeReceiveRequest(tABC_WalletID self,
                                     const char *szRequestID,
                                     tABC_Error *pError);

tABC_CC ABC_TxCancelReceiveRequest(tABC_WalletID self,
                                   const char *szRequestID,
                                   tABC_Error *pError);

tABC_CC ABC_TxGenerateRequestQRCode(tABC_WalletID self,
                                    const char *szRequestID,
                                    char **pszURI,
                                    unsigned char **paData,
                                    unsigned int *pWidth,
                                    tABC_Error *pError);

tABC_CC ABC_TxGetTransaction(tABC_WalletID self,
                             const char *szID,
                             tABC_TxInfo **ppTransaction,
                             tABC_Error *pError);

tABC_CC ABC_TxGetTransactions(tABC_WalletID self,
                              int64_t startTime,
                              int64_t endTime,
                              tABC_TxInfo ***paTransactions,
                              unsigned int *pCount,
                              tABC_Error *pError);

tABC_CC ABC_TxSearchTransactions(tABC_WalletID self,
                                 const char *szQuery,
                                 tABC_TxInfo ***paTransactions,
                                 unsigned int *pCount,
                                 tABC_Error *pError);

void ABC_TxFreeTransaction(tABC_TxInfo *pTransactions);

void ABC_TxFreeTransactions(tABC_TxInfo **aTransactions,
                            unsigned int count);

tABC_CC ABC_TxSetTransactionDetails(tABC_WalletID self,
                                    const char *szID,
                                    tABC_TxDetails *pDetails,
                                    tABC_Error *pError);

tABC_CC ABC_TxGetTransactionDetails(tABC_WalletID self,
                                    const char *szID,
                                    tABC_TxDetails **ppDetails,
                                    tABC_Error *pError);

tABC_CC ABC_TxGetRequestAddress(tABC_WalletID self,
                                const char *szRequestID,
                                char **pszAddress,
                                tABC_Error *pError);

tABC_CC ABC_TxGetPendingRequests(tABC_WalletID self,
                                 tABC_RequestInfo ***paRequests,
                                 unsigned int *pCount,
                                 tABC_Error *pError);

void ABC_TxFreeRequests(tABC_RequestInfo **aRequests,
                        unsigned int count);

tABC_CC ABC_TxSweepSaveTransaction(tABC_WalletID wallet,
                                   const char *txId,
                                   const char *malTxId,
                                   uint64_t funds,
                                   tABC_TxDetails *pDetails,
                                   tABC_Error *pError);

// Blocking functions:
tABC_CC  ABC_TxSend(tABC_TxSendInfo *pInfo,
                    char **pszUUID,
                    tABC_Error *pError);

tABC_CC ABC_TxSendComplete(tABC_TxSendInfo *pInfo,
                           tABC_UnsignedTx *utx,
                           tABC_Error *pError);

tABC_CC  ABC_TxCalcSendFees(tABC_TxSendInfo *pInfo,
                            int64_t *pTotalFees,
                            tABC_Error *pError);

tABC_CC ABC_TxGetPubAddresses(tABC_WalletID self,
                            char ***paAddresses,
                            unsigned int *pCount,
                            tABC_Error *pError);

tABC_CC ABC_TxWatchAddresses(tABC_WalletID self,
                             tABC_Error *pError);

} // namespace abcd

#endif
