/**
 * @file
 * AirBitz Tx function prototypes
 *
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
 *
 *  @author See AUTHORS
 *  @version 1.0
 */

#ifndef ABC_Tx_h
#define ABC_Tx_h

#include "ABC.h"
#include "util/ABC_Util.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * AirBitz Core Send Tx Structure
     *
     * This structure contains the detailed information associated
     * with initiating a send.
     *
     */
    typedef struct sABC_TxSendInfo
    {
        char                    *szUserName;
        char                    *szPassword;
        char                    *szWalletUUID;
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
                                const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                const char *szDestAddress,
                                const tABC_TxDetails *pDetails,
                                tABC_Error *pError);


    void ABC_TxSendInfoFree(tABC_TxSendInfo *pTxSendInfo);

    double ABC_TxSatoshiToBitcoin(int64_t satoshi);

    int64_t ABC_TxBitcoinToSatoshi(double bitcoin);

    tABC_CC ABC_TxSatoshiToCurrency(const char *szUserName,
                                    const char *szPassword,
                                    int64_t satoshi,
                                    double *pCurrency,
                                    int currencyNum,
                                    tABC_Error *pError);

    tABC_CC ABC_TxCurrencyToSatoshi(const char *szUserName,
                                    const char *szPassword,
                                    double currency,
                                    int currencyNum,
                                    int64_t *pSatoshi,
                                    tABC_Error *pError);

    tABC_CC ABC_TxBlockHeightUpdate(uint64_t height,
                                    tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                                    void *pData,
                                    tABC_Error *pError);

    tABC_CC ABC_TxReceiveTransaction(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     uint64_t amountSatoshi, uint64_t feeSatoshi,
                                     tABC_TxOutput **paInAddress, unsigned int inAddressCount,
                                     tABC_TxOutput **paOutAddresses, unsigned int outAddressCount,
                                     const char *szTxId, const char *szMalTxId,
                                     tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                                     void *pData,
                                     tABC_Error *pError);

    tABC_CC ABC_TxCreateInitialAddresses(const char *szUserName,
                                         const char *szPassword,
                                         const char *szWalletUUID,
                                         tABC_Error *pError);

    tABC_CC ABC_TxCreateReceiveRequest(const char *szUserName,
                                       const char *szPassword,
                                       const char *szWalletUUID,
                                       tABC_TxDetails *pDetails,
                                       char **pszRequestID,
                                       bool bTransfer,
                                       tABC_Error *pError);

    tABC_CC ABC_TxModifyReceiveRequest(const char *szUserName,
                                       const char *szPassword,
                                       const char *szWalletUUID,
                                       const char *szRequestID,
                                       tABC_TxDetails *pDetails,
                                       tABC_Error *pError);

    tABC_CC ABC_TxFinalizeReceiveRequest(const char *szUserName,
                                         const char *szPassword,
                                         const char *szWalletUUID,
                                         const char *szRequestID,
                                         tABC_Error *pError);

    tABC_CC ABC_TxCancelReceiveRequest(const char *szUserName,
                                       const char *szPassword,
                                       const char *szWalletUUID,
                                       const char *szRequestID,
                                       tABC_Error *pError);

    tABC_CC ABC_TxGenerateRequestQRCode(const char *szUserName,
                                        const char *szPassword,
                                        const char *szWalletUUID,
                                        const char *szRequestID,
                                        char **pszURI,
                                        unsigned char **paData,
                                        unsigned int *pWidth,
                                        tABC_Error *pError);

    tABC_CC ABC_TxGetTransaction(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 const char *szID,
                                 tABC_TxInfo **ppTransaction,
                                 tABC_Error *pError);

    tABC_CC ABC_TxGetTransactions(const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletUUID,
                                  int64_t startTime,
                                  int64_t endTime,
                                  tABC_TxInfo ***paTransactions,
                                  unsigned int *pCount,
                                  tABC_Error *pError);

    tABC_CC ABC_TxSearchTransactions(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     const char *szQuery,
                                     tABC_TxInfo ***paTransactions,
                                     unsigned int *pCount,
                                     tABC_Error *pError);

    void ABC_TxFreeTransaction(tABC_TxInfo *pTransactions);

    void ABC_TxFreeTransactions(tABC_TxInfo **aTransactions,
                                unsigned int count);

    tABC_CC ABC_TxSetTransactionDetails(const char *szUserName,
                                        const char *szPassword,
                                        const char *szWalletUUID,
                                        const char *szID,
                                        tABC_TxDetails *pDetails,
                                        tABC_Error *pError);

    tABC_CC ABC_TxGetTransactionDetails(const char *szUserName,
                                        const char *szPassword,
                                        const char *szWalletUUID,
                                        const char *szID,
                                        tABC_TxDetails **ppDetails,
                                        tABC_Error *pError);

    tABC_CC ABC_TxGetRequestAddress(const char *szUserName,
                                    const char *szPassword,
                                    const char *szWalletUUID,
                                    const char *szRequestID,
                                    char **pszAddress,
                                    tABC_Error *pError);

    tABC_CC ABC_TxGetPendingRequests(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     tABC_RequestInfo ***paRequests,
                                     unsigned int *pCount,
                                     tABC_Error *pError);

    void ABC_TxFreeRequests(tABC_RequestInfo **aRequests,
                            unsigned int count);

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

    tABC_CC ABC_TxGetPubAddresses(const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                char ***paAddresses,
                                unsigned int *pCount,
                                tABC_Error *pError);

    tABC_CC ABC_TxGetPrivAddresses(const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   tABC_U08Buf seed,
                                   char ***paAddresses,
                                   unsigned int *pCount,
                                   tABC_Error *pError);

    tABC_CC ABC_TxWatchAddresses(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
