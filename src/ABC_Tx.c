/**
 * @file
 * AirBitz Tx functions.
 *
 * This file contains all of the functions associated with transaction creation,
 * viewing and modification.
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

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <qrencode.h>
#include "ABC_Tx.h"
#include "ABC_Exchanges.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Crypto.h"
#include "ABC_Login.h"
#include "ABC_Mutex.h"
#include "ABC_Wallet.h"
#include "ABC_Debug.h"
#include "ABC_Bridge.h"

#include <unistd.h> // for fake code
#include <pthread.h>

#define SATOSHI_PER_BITCOIN                     100000000

#define WATCH_ADDITIONAL_ADDRESSES              10

#define TX_MAX_ADDR_ID_LENGTH                   20 // largest char count for the string version of the id number - 20 digits should handle it

#define TX_MAX_AMOUNT_LENGTH                    100 // should be max length of a bit coin amount string
#define TX_MAX_CATEGORY_LENGTH                  512

#define TX_INTERNAL_SUFFIX                      "-int.json" // the transaction was created by our direct action (i.e., send)
#define TX_EXTERNAL_SUFFIX                      "-ext.json" // the transaction was created due to events in the block-chain (usually receives)

#define ADDRESS_FILENAME_SEPARATOR              '-'
#define ADDRESS_FILENAME_SUFFIX                 ".json"
#define ADDRESS_FILENAME_MIN_LEN                8 // <id>-<public_addr>.json

#define JSON_DETAILS_FIELD                      "meta"
#define JSON_CREATION_DATE_FIELD                "creationDate"
#define JSON_MALLEABLE_TX_ID                    "malleableTxId"
#define JSON_AMOUNT_SATOSHI_FIELD               "amountSatoshi"
#define JSON_AMOUNT_AIRBITZ_FEE_SATOSHI_FIELD   "amountFeeAirBitzSatoshi"
#define JSON_AMOUNT_MINERS_FEE_SATOSHI_FIELD    "amountFeeMinersSatoshi"

#define JSON_TX_ID_FIELD                        "ntxid"
#define JSON_TX_STATE_FIELD                     "state"
#define JSON_TX_INTERNAL_FIELD                  "internal"
#define JSON_TX_LOGIN_FIELD                     "login"
#define JSON_TX_AMOUNT_CURRENCY_FIELD           "amountCurrency"
#define JSON_TX_NAME_FIELD                      "name"
#define JSON_TX_BIZID_FIELD                     "bizId"
#define JSON_TX_CATEGORY_FIELD                  "category"
#define JSON_TX_NOTES_FIELD                     "notes"
#define JSON_TX_ATTRIBUTES_FIELD                "attributes"
#define JSON_TX_OUTPUTS_FIELD                   "outputs"
#define JSON_TX_OUTPUT_FLAG                     "input"
#define JSON_TX_OUTPUT_VALUE                    "value"
#define JSON_TX_OUTPUT_ADDRESS                  "address"
#define JSON_TX_OUTPUT_TXID                     "txid"
#define JSON_TX_OUTPUT_INDEX                    "index"

#define JSON_ADDR_SEQ_FIELD                     "seq"
#define JSON_ADDR_ADDRESS_FIELD                 "address"
#define JSON_ADDR_STATE_FIELD                   "state"
#define JSON_ADDR_RECYCLEABLE_FIELD             "recycleable"
#define JSON_ADDR_ACTIVITY_FIELD                "activity"
#define JSON_ADDR_DATE_FIELD                    "date"

typedef enum eTxType
{
    TxType_None = 0,
    TxType_Internal,
    TxType_External
} tTxType;

typedef struct sTxStateInfo
{
    int64_t timeCreation;
    bool    bInternal;
    char    *szMalleableTxId;
} tTxStateInfo;

typedef struct sABC_Tx
{
    char            *szID; // ntxid from bitcoin
    tABC_TxDetails  *pDetails;
    tTxStateInfo    *pStateInfo;
    unsigned int    countOutputs;
    tABC_TxOutput   **aOutputs;
} tABC_Tx;

typedef struct sTxAddressActivity
{
    char    *szTxID; // ntxid from bitcoin associated with this activity
    int64_t timeCreation;
    int64_t amountSatoshi;
} tTxAddressActivity;

typedef struct sTxAddressStateInfo
{
    int64_t             timeCreation;
    bool                bRecycleable;
    unsigned int        countActivities;
    tTxAddressActivity  *aActivities;
} tTxAddressStateInfo;

typedef struct sABC_TxAddress
{
    int32_t             seq; // sequence number
    char                *szID; // sequence number in string form
    char                *szPubAddress; // public address
    tABC_TxDetails      *pDetails;
    tTxAddressStateInfo *pStateInfo;
} tABC_TxAddress;

/** globally accessable function pointer for BitCoin event callbacks */
static tABC_BitCoin_Event_Callback gfAsyncBitCoinEventCallback = NULL;
static void *pAsyncBitCoinCallerData = NULL;

static tABC_CC  ABC_TxCreateNewAddress(const char *szUserName, const char *szPassword, const char *szWalletUUID, tABC_TxDetails *pDetails, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_GetAddressFilename(const char *szWalletUUID, const char *szRequestID, char **pszFilename, tABC_Error *pError);
static tABC_CC  ABC_TxParseAddrFilename(const char *szFilename, char **pszID, char **pszPublicAddress, tABC_Error *pError);
static tABC_CC  ABC_TxSetAddressRecycle(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szAddress, bool bRecyclable, tABC_Error *pError);
static tABC_CC  ABC_TxCheckForInternalEquivalent(const char *szFilename, bool *pbEquivalent, tABC_Error *pError);
static tABC_CC  ABC_TxGetTxTypeAndBasename(const char *szFilename, tTxType *pType, char **pszBasename, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTransactionInfo(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szFilename, tABC_TxInfo **ppTransaction, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTxAndAppendToArray(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szFilename, tABC_TxInfo ***paTransactions, unsigned int *pCount, tABC_Error *pError);
static tABC_CC  ABC_TxGetAddressOwed(tABC_TxAddress *pAddr, int64_t *pSatoshiBalance, tABC_Error *pError);
static tABC_CC  ABC_TxDefaultRequestDetails(const char *szUserName, const char *szPassword, tABC_TxDetails *pDetails, tABC_Error *pError);
static void     ABC_TxFreeRequest(tABC_RequestInfo *pRequest);
static tABC_CC  ABC_TxCreateTxFilename(char **pszFilename, const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szTxID, bool bInternal, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTransaction(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szFilename, tABC_Tx **ppTx, tABC_Error *pError);
static tABC_CC  ABC_TxFindRequest(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szMatchAddress, tABC_TxAddress **ppMatched, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeTxState(json_t *pJSON_Obj, tTxStateInfo **ppInfo, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails **ppDetails, tABC_Error *pError);
static void     ABC_TxFreeTx(tABC_Tx *pTx);
static tABC_CC  ABC_TxCreateTxDir(const char *szWalletUUID, tABC_Error *pError);
static tABC_CC  ABC_TxSaveTransaction(const char *szUserName, const char *szPassword, const char *szWalletUUID, const tABC_Tx *pTx, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeTxState(json_t *pJSON_Obj, tTxStateInfo *pInfo, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails *pDetails, tABC_Error *pError);
static int      ABC_TxInfoPtrCompare (const void * a, const void * b);
static tABC_CC  ABC_TxLoadAddress(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szAddressID, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_TxLoadAddressFile(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szFilename, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo **ppState, tABC_Error *pError);
static tABC_CC  ABC_TxSaveAddress(const char *szUserName, const char *szPassword, const char *szWalletUUID, const tABC_TxAddress *pAddress, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo *pInfo, tABC_Error *pError);
static tABC_CC  ABC_TxCreateAddressFilename(char **pszFilename, const char *szUserName, const char *szPassword, const char *szWalletUUID, const tABC_TxAddress *pAddress, tABC_Error *pError);
static tABC_CC  ABC_TxCreateAddressDir(const char *szWalletUUID, tABC_Error *pError);
static void     ABC_TxFreeAddress(tABC_TxAddress *pAddress);
static void     ABC_TxFreeAddressStateInfo(tTxAddressStateInfo *pInfo);
static void     ABC_TxFreeAddresses(tABC_TxAddress **aAddresses, unsigned int count);
static void     ABC_TxFreeOutput(tABC_TxOutput *pOutputs);
static void     ABC_TxFreeOutputs(tABC_TxOutput **aOutputs, unsigned int count);
static tABC_CC  ABC_TxGetAddresses(const char *szUserName, const char *szPassword, const char *szWalletUUID, tABC_TxAddress ***paAddresses, unsigned int *pCount, tABC_Error *pError);
static int      ABC_TxAddrPtrCompare(const void * a, const void * b);
static tABC_CC  ABC_TxLoadAddressAndAppendToArray(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szFilename, tABC_TxAddress ***paAddresses, unsigned int *pCount, tABC_Error *pError);
//static void     ABC_TxPrintAddresses(tABC_TxAddress **aAddresses, unsigned int count);
static tABC_CC  ABC_TxMutexLock(tABC_Error *pError);
static tABC_CC  ABC_TxMutexUnlock(tABC_Error *pError);
static tABC_CC  ABC_TxAddressAddTx(tABC_TxAddress *pAddress, tABC_Tx *pTx, tABC_Error *pError);
static tABC_CC  ABC_TxTransactionExists(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szID, tABC_Tx **pTx, tABC_Error *pError);
static void     ABC_TxStrTable(const char *needle, int *table);
static int      ABC_TxStrStr(const char *haystack, const char *needle, tABC_Error *pError);
static int      ABC_TxCopyOuputs(tABC_Tx *pTx, tABC_TxOutput **aOutputs, int countOutputs, tABC_Error *pError);
static int      ABC_TxTransferPopulate(tABC_TxSendInfo *pInfo, tABC_Tx *pTx, tABC_Tx *pReceiveTx, tABC_Error *pError);
static tABC_CC  ABC_TxWalletOwnsAddress(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szAddress, bool *bFound, tABC_Error *pError);


// fake code:
#if NETWORK_FAKE
static tABC_CC  ABC_TxKickoffFakeReceive(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szAddress, tABC_Error *pError);
static void     *ABC_TxFakeReceiveThread(void *);
static tABC_CC  ABC_TxFakeSend(tABC_TxSendInfo  *pInfo, char **pszTxID, tABC_Error *pError);
#endif

/**
 * Initializes the
 */
tABC_CC ABC_TxInitialize(tABC_BitCoin_Event_Callback  fAsyncBitCoinEventCallback,
                         void                         *pData,
                         tABC_Error                   *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    gfAsyncBitCoinEventCallback = fAsyncBitCoinEventCallback;
    pAsyncBitCoinCallerData = pData;

    return cc;
}

/**
 * Allocate a send info struct and populate it with the data given
 */
tABC_CC ABC_TxSendInfoAlloc(tABC_TxSendInfo **ppTxSendInfo,
                            const char *szUserName,
                            const char *szPassword,
                            const char *szWalletUUID,
                            const char *szDestAddress,
                            const tABC_TxDetails *pDetails,
                            tABC_Request_Callback fRequestCallback,
                            void *pData,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxSendInfo *pTxSendInfo = NULL;

    ABC_CHECK_NULL(ppTxSendInfo);
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_NULL(szDestAddress);
    ABC_CHECK_NULL(pDetails);
//    ABC_CHECK_NULL(fRequestCallback);

    ABC_ALLOC(pTxSendInfo, sizeof(tABC_TxSendInfo));

    ABC_STRDUP(pTxSendInfo->szUserName, szUserName);
    ABC_STRDUP(pTxSendInfo->szPassword, szPassword);
    ABC_STRDUP(pTxSendInfo->szWalletUUID, szWalletUUID);
    ABC_STRDUP(pTxSendInfo->szDestAddress, szDestAddress);

    ABC_CHECK_RET(ABC_TxDupDetails(&(pTxSendInfo->pDetails), pDetails, pError));

    pTxSendInfo->fRequestCallback = fRequestCallback;

    pTxSendInfo->pData = pData;

    *ppTxSendInfo = pTxSendInfo;
    pTxSendInfo = NULL;

exit:
    ABC_TxSendInfoFree(pTxSendInfo);

    return cc;
}

/**
 * Free a send info struct
 */
void ABC_TxSendInfoFree(tABC_TxSendInfo *pTxSendInfo)
{
    if (pTxSendInfo)
    {
        ABC_FREE_STR(pTxSendInfo->szUserName);
        ABC_FREE_STR(pTxSendInfo->szPassword);
        ABC_FREE_STR(pTxSendInfo->szWalletUUID);
        ABC_FREE_STR(pTxSendInfo->szDestAddress);

        ABC_FREE_STR(pTxSendInfo->szDestWalletUUID);
        ABC_FREE_STR(pTxSendInfo->szDestName);
        ABC_FREE_STR(pTxSendInfo->szDestCategory);

        ABC_FREE_STR(pTxSendInfo->szSrcName);
        ABC_FREE_STR(pTxSendInfo->szSrcCategory);

        ABC_TxFreeDetails(pTxSendInfo->pDetails);

        ABC_CLEAR_FREE(pTxSendInfo, sizeof(tABC_TxSendInfo));
    }
}

/**
 * Sends a transaction. Assumes it is running in a thread.
 *
 * This function sends a transaction.
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to send a transaction (should be a tABC_TxSendInfo)
 */
void *ABC_TxSendThreaded(void *pData)
{
    tABC_TxSendInfo *pInfo = (tABC_TxSendInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;
        memset(&results, 0, sizeof(tABC_RequestResults));

        results.requestType = ABC_RequestType_SendBitcoin;

        results.bSuccess = false;

        // send the transaction
        tABC_CC CC = ABC_TxSend(pInfo, (char **) &(results.pRetData), &(results.errorInfo));
        results.errorInfo.code = CC;

        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.szWalletUUID = strdup(pInfo->szWalletUUID);
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);

        // it is our responsibility to free the info struct
        ABC_TxSendInfoFree(pInfo);
    }

    return NULL;
}

/**
 * Sends the transaction with the given info.
 *
 * @param pInfo Pointer to transaction information
 * @param pszTxID Pointer to hold allocated pointer to transaction ID string
 */
tABC_CC ABC_TxSend(tABC_TxSendInfo  *pInfo,
                   char             **pszTxID,
                   tABC_Error       *pError)
{
#if NETWORK_FAKE
    return ABC_TxFakeSend(pInfo, pszTxID, pError);
#else
    tABC_CC cc = ABC_CC_Ok;
    tABC_U08Buf privSeed = ABC_BUF_NULL;
    tABC_Tx *pTx = NULL;
    tABC_Tx *pReceiveTx = NULL;
    tABC_UnsignedTx utx;
    bool bFound = false;
    tABC_WalletInfo *pWallet = NULL;
    double Currency;

    // Change address variables
    tABC_TxAddress *pChangeAddr = NULL;

    char *szPrivSeed = NULL;
    char **paAddresses = NULL;
    char **paPrivAddresses = NULL;
    unsigned int countAddresses = 0, privCountAddresses = 0;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(pInfo);
    ABC_CHECK_NULL(pszTxID);

    // take this non-blocking opportunity to update the info from the server if needed
    ABC_CHECK_RET(ABC_GeneralUpdateInfo(pError));

    // find/create a change address
    ABC_CHECK_RET(ABC_TxCreateNewAddress(
                    pInfo->szUserName, pInfo->szPassword,
                    pInfo->szWalletUUID, pInfo->pDetails,
                    &pChangeAddr, pError));
    pChangeAddr->pStateInfo->bRecycleable = false;
    // save out this address
    ABC_CHECK_RET(ABC_TxSaveAddress(pInfo->szUserName, pInfo->szPassword,
                                    pInfo->szWalletUUID, pChangeAddr, pError));

    // Fetch addresses for this wallet
    ABC_CHECK_RET(
        ABC_TxGetPubAddresses(pInfo->szUserName, pInfo->szPassword,
                              pInfo->szWalletUUID,
                              &paAddresses, &countAddresses, pError));
    // Make an unsigned transaction
    ABC_CHECK_RET(
        ABC_BridgeTxMake(pInfo, paAddresses, countAddresses,
                         pChangeAddr->szPubAddress, &utx, pError));

    // Fetch Private Seed
    ABC_CHECK_RET(
        ABC_WalletGetBitcoinPrivateSeed(
            pInfo->szUserName, pInfo->szPassword,
            pInfo->szWalletUUID, &privSeed, pError));
    // Fetch the private addresses
    ABC_CHECK_RET(
        ABC_TxGetPrivAddresses(pInfo->szUserName, pInfo->szPassword,
                               pInfo->szWalletUUID, privSeed,
                               &paPrivAddresses, &privCountAddresses,
                               pError));
    // Sign the transaction
    ABC_CHECK_RET(
        ABC_BridgeTxSignSend(pInfo, paPrivAddresses, privCountAddresses,
                             &utx, pError));

    // Start watching all addresses incuding new change addres
    ABC_CHECK_RET(ABC_TxWatchAddresses(pInfo->szUserName, pInfo->szPassword,
                                       pInfo->szWalletUUID, pError));

    // sucessfully sent, now create a transaction
    ABC_ALLOC(pTx, sizeof(tABC_Tx));
    ABC_ALLOC(pTx->pStateInfo, sizeof(tTxStateInfo));

    // set the state
    pTx->pStateInfo->timeCreation = time(NULL);
    pTx->pStateInfo->bInternal = true;
    ABC_STRDUP(pTx->pStateInfo->szMalleableTxId, utx.szTxMalleableId);
    // Copy outputs
    ABC_TxCopyOuputs(pTx, utx.aOutputs, utx.countOutputs, pError);
    // copy the details
    ABC_CHECK_RET(ABC_TxDupDetails(&(pTx->pDetails), pInfo->pDetails, pError));
    // Add in tx fees to the amount of the tx

    ABC_CHECK_RET(ABC_TxWalletOwnsAddress(pInfo->szUserName, pInfo->szPassword,
                                          pInfo->szWalletUUID, pInfo->szDestAddress,
                                          &bFound, pError));
    if (bFound)
    {
        pTx->pDetails->amountSatoshi = pInfo->pDetails->amountFeesAirbitzSatoshi
                                        + pInfo->pDetails->amountFeesMinersSatoshi;

    }
    else
    {
        pTx->pDetails->amountSatoshi = pInfo->pDetails->amountSatoshi
                                        + pInfo->pDetails->amountFeesAirbitzSatoshi
                                        + pInfo->pDetails->amountFeesMinersSatoshi;
    }
    ABC_CHECK_RET(ABC_GetWalletInfo(pInfo->szUserName, pInfo->szPassword,
                                    pInfo->szWalletUUID, &pWallet, pError));
    ABC_CHECK_RET(ABC_SatoshiToCurrency(
                    pInfo->szUserName, pInfo->szPassword,
                    pTx->pDetails->amountSatoshi, &Currency,
                    pWallet->currencyNum, pError));
    pTx->pDetails->amountCurrency = Currency;

    if (pTx->pDetails->amountSatoshi > 0)
        pTx->pDetails->amountSatoshi *= -1;
    if (pTx->pDetails->amountCurrency > 0)
        pTx->pDetails->amountCurrency *= -1.0;

    // Store transaction ID
    ABC_STRDUP(pTx->szID, utx.szTxId);
    if (pInfo->bTransfer)
    {
        ABC_ALLOC(pReceiveTx, sizeof(tABC_Tx));
        ABC_ALLOC(pReceiveTx->pStateInfo, sizeof(tTxStateInfo));

        // set the state
        pReceiveTx->pStateInfo->timeCreation = time(NULL);
        pReceiveTx->pStateInfo->bInternal = true;
        ABC_STRDUP(pReceiveTx->pStateInfo->szMalleableTxId, utx.szTxMalleableId);
        // Copy outputs
        ABC_TxCopyOuputs(pReceiveTx, utx.aOutputs, utx.countOutputs, pError);
        // copy the details
        ABC_CHECK_RET(ABC_TxDupDetails(&(pReceiveTx->pDetails), pInfo->pDetails, pError));

        pReceiveTx->pDetails->amountSatoshi = pInfo->pDetails->amountSatoshi;

        //
        // Since this wallet is receiving, it didn't really get charged AB fees
        // This should really be an assert since no transfers should have AB fees
        //
        pReceiveTx->pDetails->amountFeesAirbitzSatoshi = 0;

        if (pReceiveTx->pDetails->amountSatoshi < 0)
            pReceiveTx->pDetails->amountSatoshi *= -1;
        if (pReceiveTx->pDetails->amountCurrency < 0)
            pReceiveTx->pDetails->amountCurrency *= -1.0;

        // Store transaction ID
        ABC_STRDUP(pReceiveTx->szID, utx.szTxId);

        // Set the payee and category for both txs
        ABC_CHECK_RET(ABC_TxTransferPopulate(pInfo, pTx, pReceiveTx, pError));

        // save the transaction
        ABC_CHECK_RET(ABC_TxSaveTransaction(pInfo->szUserName, pInfo->szPassword,
                                            pInfo->szDestWalletUUID, pReceiveTx, pError));
    }

    // save the transaction
    ABC_CHECK_RET(
        ABC_TxSaveTransaction(pInfo->szUserName, pInfo->szPassword,
                              pInfo->szWalletUUID, pTx, pError));

    // sync the data
    ABC_CHECK_RET(ABC_DataSyncAll(pInfo->szUserName, pInfo->szPassword, pError));

    // set the transaction id for the caller
    ABC_STRDUP(*pszTxID, pTx->szID);

exit:
    ABC_FREE(szPrivSeed);
    ABC_TxFreeTx(pTx);
    ABC_TxFreeTx(pReceiveTx);
    ABC_TxFreeAddress(pChangeAddr);
    ABC_UtilFreeStringArray(paAddresses, countAddresses);
    ABC_TxFreeOutputs(utx.aOutputs, utx.countOutputs);
    ABC_TxMutexUnlock(NULL);
    return cc;
#endif
}

tABC_CC  ABC_TxCalcSendFees(tABC_TxSendInfo *pInfo, int64_t *pTotalFees, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_UnsignedTx utx;
    tABC_TxAddress *pChangeAddr = NULL;

    char **paAddresses = NULL;
    unsigned int countAddresses = 0;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(pInfo);

    pInfo->pDetails->amountFeesAirbitzSatoshi = 0;
    pInfo->pDetails->amountFeesMinersSatoshi = 0;

    // find/create a change address
    ABC_CHECK_RET(ABC_TxCreateNewAddress(
                    pInfo->szUserName, pInfo->szPassword,
                    pInfo->szWalletUUID, pInfo->pDetails,
                    &pChangeAddr, pError));
    // save out this address
    ABC_CHECK_RET(ABC_TxSaveAddress(pInfo->szUserName, pInfo->szPassword,
                                    pInfo->szWalletUUID, pChangeAddr, pError));

    // Fetch addresses for this wallet
    ABC_CHECK_RET(
        ABC_TxGetPubAddresses(pInfo->szUserName, pInfo->szPassword,
                              pInfo->szWalletUUID,
                              &paAddresses, &countAddresses, pError));
    cc = ABC_BridgeTxMake(pInfo, paAddresses, countAddresses,
                            pChangeAddr->szPubAddress, &utx, pError);
    *pTotalFees = pInfo->pDetails->amountFeesAirbitzSatoshi
                + pInfo->pDetails->amountFeesMinersSatoshi;
    ABC_CHECK_RET(cc);
exit:
    ABC_UtilFreeStringArray(paAddresses, countAddresses);
    ABC_TxFreeAddress(pChangeAddr);
    ABC_TxMutexUnlock(NULL);
    return cc;
}

tABC_CC ABC_TxWalletOwnsAddress(const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                const char *szAddress,
                                bool *bFound,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char **paAddresses = NULL;
    unsigned int countAddresses = 0;
    *bFound = false;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_NULL(szAddress);

    ABC_CHECK_RET(
        ABC_TxGetPubAddresses(szUserName, szPassword, szWalletUUID,
                              &paAddresses, &countAddresses, pError));
    for (int i = 0; i < countAddresses; ++i)
    {
        if (strncmp(szAddress, paAddresses[i], strlen(szAddress)) == 0)
        {
            *bFound = true;
            break;
        }
    }
exit:
    ABC_UtilFreeStringArray(paAddresses, countAddresses);
    return cc;
}

/**
 * Gets the public addresses associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param paAddresses       Pointer to string array of addresses
 * @param pCount            Pointer to store number of addresses
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetPubAddresses(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              char ***paAddresses,
                              unsigned int *pCount,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_TxAddress **aAddresses = NULL;
    char **sAddresses;
    unsigned int countAddresses = 0;
    ABC_CHECK_RET(
        ABC_TxGetAddresses(szUserName, szPassword, szWalletUUID,
                           &aAddresses, &countAddresses, pError));
    ABC_ALLOC(sAddresses, sizeof(char *) * countAddresses);
    for (int i = 0; i < countAddresses; i++)
    {
        const char *s = aAddresses[i]->szPubAddress;
        ABC_STRDUP(sAddresses[i], s);
    }
    *pCount = countAddresses;
    *paAddresses = sAddresses;
exit:
    ABC_TxFreeAddresses(aAddresses, countAddresses);
    return cc;
}

/**
 * Gets the private keys with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param paAddresses       Pointer to string array of addresses
 * @param pCount            Pointer to store number of addresses
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetPrivAddresses(const char *szUserName,
                               const char *szPassword,
                               const char *szWalletUUID,
                               tABC_U08Buf seed,
                               char ***paAddresses,
                               unsigned int *pCount,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_TxAddress **aAddresses = NULL;
    char **sAddresses;
    unsigned int countAddresses = 0;
    ABC_CHECK_RET(
        ABC_TxGetAddresses(szUserName, szPassword, szWalletUUID,
                           &aAddresses, &countAddresses, pError));
    ABC_ALLOC(sAddresses, sizeof(char *) * countAddresses);
    for (int i = 0; i < countAddresses; i++)
    {
        ABC_CHECK_RET(
            ABC_BridgeGetBitcoinPrivAddress(&sAddresses[i],
                                            seed,
                                            aAddresses[i]->seq,
                                            pError));
        ABC_DebugLog("Private Seed N %s %d %s\n", aAddresses[i]->szPubAddress,
                                                  aAddresses[i]->seq,
                                                  sAddresses[i]);
    }
    *pCount = countAddresses;
    *paAddresses = sAddresses;
exit:
    ABC_TxFreeAddresses(aAddresses, countAddresses);
    return cc;
}

tABC_CC ABC_TxWatchAddresses(const char *szUserName,
                             const char *szPassword,
                             const char *szWalletUUID,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf Seed            = ABC_BUF_NULL;
    char *szPubAddress          = NULL;
    tABC_TxAddress **aAddresses = NULL;
    unsigned int countAddresses = 0;
    int64_t N                   = -1;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_RET(
        ABC_TxGetAddresses(szUserName, szPassword, szWalletUUID,
                           &aAddresses, &countAddresses, pError));
    for (int i = 0; i < countAddresses; i++)
    {
        const tABC_TxAddress *a = aAddresses[i];
        ABC_CHECK_RET(
            ABC_BridgeWatchAddr(szUserName, szPassword, szWalletUUID,
                                a->szPubAddress, false, pError));
        if (a->seq > N)
        {
            N = a->seq;
        }
    }

    // Fetch private Seed
    ABC_CHECK_RET(
        ABC_WalletGetBitcoinPrivateSeed(szUserName, szPassword,
                                        szWalletUUID, &Seed, pError));
    // watch additional addresses
    for (int i = 0; i < WATCH_ADDITIONAL_ADDRESSES; ++i)
    {
        ABC_CHECK_RET(
            ABC_BridgeGetBitcoinPubAddress(&szPubAddress,
                                           Seed,
                                           countAddresses + i,
                                           pError));
        ABC_CHECK_RET(
            ABC_BridgeWatchAddr(szUserName, szPassword, szWalletUUID,
                                szPubAddress, false, pError));
        ABC_FREE_STR(szPubAddress);
    }
exit:
    ABC_TxFreeAddresses(aAddresses, countAddresses);
    ABC_FREE_STR(szPubAddress);

    ABC_CHECK_RET(ABC_TxMutexUnlock(pError));
    return cc;
}


/**
 * Duplicate a TX details struct
 */
tABC_CC ABC_TxDupDetails(tABC_TxDetails **ppNewDetails, const tABC_TxDetails *pOldDetails, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxDetails *pNewDetails = NULL;

    ABC_CHECK_NULL(ppNewDetails);
    ABC_CHECK_NULL(pOldDetails);

    ABC_ALLOC(pNewDetails, sizeof(tABC_TxDetails));

    pNewDetails->amountSatoshi  = pOldDetails->amountSatoshi;
    pNewDetails->amountFeesAirbitzSatoshi = pOldDetails->amountFeesAirbitzSatoshi;
    pNewDetails->amountFeesMinersSatoshi = pOldDetails->amountFeesMinersSatoshi;
    pNewDetails->amountCurrency  = pOldDetails->amountCurrency;
    pNewDetails->bizId = pOldDetails->bizId;
    pNewDetails->attributes = pOldDetails->attributes;
    if (pOldDetails->szName != NULL)
    {
        ABC_STRDUP(pNewDetails->szName, pOldDetails->szName);
    }
    if (pOldDetails->szCategory != NULL)
    {
        ABC_STRDUP(pNewDetails->szCategory, pOldDetails->szCategory);
    }
    if (pOldDetails->szNotes != NULL)
    {
        ABC_STRDUP(pNewDetails->szNotes, pOldDetails->szNotes);
    }

    // set the pointer for the caller
    *ppNewDetails = pNewDetails;
    pNewDetails = NULL;

exit:
    ABC_TxFreeDetails(pNewDetails);

    return cc;
}

/**
 * Free a TX details struct
 */
void ABC_TxFreeDetails(tABC_TxDetails *pDetails)
{
    if (pDetails)
    {
        ABC_FREE_STR(pDetails->szName);
        ABC_FREE_STR(pDetails->szCategory);
        ABC_FREE_STR(pDetails->szNotes);
        ABC_CLEAR_FREE(pDetails, sizeof(tABC_TxDetails));
    }
}

/**
 * Converts amount from Satoshi to Bitcoin
 *
 * @param satoshi Amount in Satoshi
 */
double ABC_TxSatoshiToBitcoin(int64_t satoshi)
{
    return((double) satoshi / (double) SATOSHI_PER_BITCOIN);
}

/**
 * Converts amount from Bitcoin to Satoshi
 *
 * @param bitcoin Amount in Bitcoin
 */
int64_t ABC_TxBitcoinToSatoshi(double bitcoin)
{
    return((int64_t) (bitcoin * (double) SATOSHI_PER_BITCOIN));
}

/**
 * Converts Satoshi to given currency
 *
 * @param satoshi     Amount in Satoshi
 * @param pCurrency   Pointer to location to store amount converted to currency.
 * @param currencyNum Currency ISO 4217 num
 * @param pError      A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxSatoshiToCurrency(const char *szUserName,
                                const char *szPassword,
                                int64_t satoshi,
                                double *pCurrency,
                                int currencyNum,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    double pRate;

    ABC_CHECK_NULL(pCurrency);
    *pCurrency = 0.0;

    ABC_CHECK_RET(ABC_ExchangeCurrentRate(szUserName, szPassword,
                                          currencyNum, &pRate, pError));
    *pCurrency = ABC_SatoshiToBitcoin(satoshi) * pRate;
exit:

    return cc;
}

/**
 * Converts given currency to Satoshi
 *
 * @param currency    Amount in given currency
 * @param currencyNum Currency ISO 4217 num
 * @param pSatoshi    Pointer to location to store amount converted to Satoshi
 * @param pError      A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxCurrencyToSatoshi(const char *szUserName,
                                const char *szPassword,
                                double currency,
                                int currencyNum,
                                int64_t *pSatoshi,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    double pRate;

    ABC_CHECK_NULL(pSatoshi);
    *pSatoshi = 0;

    ABC_CHECK_RET(ABC_ExchangeCurrentRate(szUserName, szPassword,
                                          currencyNum, &pRate, pError));
    *pSatoshi = ABC_BitcoinToSatoshi(currency) / pRate;
exit:

    return cc;
}

tABC_CC
ABC_TxBlockHeightUpdate(uint64_t height, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    if (gfAsyncBitCoinEventCallback)
    {
        tABC_AsyncBitCoinInfo info;
        info.eventType = ABC_AsyncEventType_BlockHeightChange;
        ABC_STRDUP(info.szDescription, "Block height change");
        gfAsyncBitCoinEventCallback(&info);
        ABC_FREE_STR(info.szDescription);
    }
exit:
    return cc;
}

/**
 * Handles creating or updating when we receive a transaction
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxReceiveTransaction(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 uint64_t amountSatoshi, uint64_t feeSatoshi,
                                 tABC_TxOutput **paInAddresses, unsigned int inAddressCount,
                                 tABC_TxOutput **paOutAddresses, unsigned int outAddressCount,
                                 const char *szTxId, const char *szMalTxId,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int i = 0;
    tABC_TxAddress *pAddress = NULL;
    tABC_U08Buf TxID = ABC_BUF_NULL;
    tABC_Tx *pTx = NULL;
    tABC_U08Buf IncomingAddress = ABC_BUF_NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));

    // Does the transaction already exist?
    ABC_TxTransactionExists(szUserName, szPassword, szWalletUUID, szTxId, &pTx, pError);
    if (pTx == NULL)
    {
        // create a transaction
        ABC_ALLOC(pTx, sizeof(tABC_Tx));
        ABC_ALLOC(pTx->pStateInfo, sizeof(tTxStateInfo));
        ABC_ALLOC(pTx->pDetails, sizeof(tABC_TxDetails));

        ABC_STRDUP(pTx->pStateInfo->szMalleableTxId, szMalTxId);
        pTx->pStateInfo->timeCreation = time(NULL);
        pTx->pDetails->amountSatoshi = amountSatoshi;
        pTx->pDetails->amountFeesMinersSatoshi = feeSatoshi;
        ABC_STRDUP(pTx->pDetails->szName, "");
        ABC_STRDUP(pTx->pDetails->szCategory, "");
        ABC_STRDUP(pTx->pDetails->szNotes, "");

        // set the state
        pTx->pStateInfo->timeCreation = time(NULL);
        pTx->pStateInfo->bInternal = false;

        // store transaction id
        ABC_STRDUP(pTx->szID, szTxId);
        // store the input addresses
        pTx->countOutputs = inAddressCount + outAddressCount;
        ABC_ALLOC(pTx->aOutputs, sizeof(tABC_TxOutput *) * pTx->countOutputs);
        for (i = 0; i < inAddressCount; ++i)
        {
            ABC_DebugLog("Saving Input address: %s\n", paInAddresses[i]->szAddress);

            ABC_ALLOC(pTx->aOutputs[i], sizeof(tABC_TxOutput));
            ABC_STRDUP(pTx->aOutputs[i]->szAddress, paInAddresses[i]->szAddress);
            ABC_STRDUP(pTx->aOutputs[i]->szTxId, paInAddresses[i]->szTxId);
            pTx->aOutputs[i]->input = paInAddresses[i]->input;
            pTx->aOutputs[i]->value = paInAddresses[i]->value;
        }
        for (i = 0; i < outAddressCount; ++i)
        {
            ABC_DebugLog("Saving Output address: %s\n", paOutAddresses[i]);
            int newi = i + inAddressCount;
            ABC_ALLOC(pTx->aOutputs[newi], sizeof(tABC_TxOutput));
            ABC_STRDUP(pTx->aOutputs[newi]->szAddress, paOutAddresses[i]->szAddress);
            ABC_STRDUP(pTx->aOutputs[newi]->szTxId, paOutAddresses[i]->szTxId);
            pTx->aOutputs[newi]->input = paOutAddresses[i]->input;
            pTx->aOutputs[newi]->value = paOutAddresses[i]->value;
        }

        // save the transaction
        ABC_CHECK_RET(
            ABC_TxSaveTransaction(szUserName, szPassword,
                                  szWalletUUID, pTx, pError));

        // add the transaction to the address
        for (i = 0; i < outAddressCount; ++i)
        {
            ABC_CHECK_RET(ABC_TxFindRequest(szUserName, szPassword,
                                            szWalletUUID,
                                            paOutAddresses[i]->szAddress,
                                            &pAddress, pError));
            if (pAddress)
            {
                ABC_CHECK_RET(ABC_TxAddressAddTx(pAddress, pTx, pError));
                pAddress->pStateInfo->bRecycleable = false;
                ABC_CHECK_RET(
                    ABC_TxSaveAddress(szUserName, szPassword,
                                    szWalletUUID, pAddress,
                                    pError));
                ABC_TxFreeAddress(pAddress);
            }
            pAddress = NULL;
        }

        if (gfAsyncBitCoinEventCallback)
        {
            tABC_AsyncBitCoinInfo info;
            info.pData = pAsyncBitCoinCallerData;
            info.eventType = ABC_AsyncEventType_IncomingBitCoin;
            ABC_STRDUP(info.szTxID, pTx->szID);
            ABC_STRDUP(info.szWalletUUID, szWalletUUID);
            ABC_STRDUP(info.szDescription, "Received funds");
            gfAsyncBitCoinEventCallback(&info);
            ABC_FREE_STR(info.szTxID);
            ABC_FREE_STR(info.szDescription);
        }
    }
    else
    {
        ABC_DebugLog("We already have %s\n", szTxId);
        // Make sure all recycle bits are set
        for (i = 0; i < outAddressCount; ++i)
        {
            ABC_CHECK_RET(ABC_TxFindRequest(
                            szUserName, szPassword, szWalletUUID,
                            paOutAddresses[i]->szAddress, &pAddress, pError));
            if (pAddress)
            {
                pAddress->pStateInfo->bRecycleable = false;
                ABC_CHECK_RET(ABC_TxSaveAddress(
                        szUserName, szPassword, szWalletUUID,
                        pAddress, pError));
                ABC_TxFreeAddress(pAddress);
            }
            pAddress = NULL;
        }
    }
exit:
    ABC_TxMutexUnlock(NULL);
    ABC_BUF_FREE(TxID);
    ABC_BUF_FREE(IncomingAddress);
    ABC_TxFreeTx(pTx);

    return cc;
}


/**
 * Creates a receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param pDetails      Pointer to transaction details
 * @param pszRequestID  Pointer to store allocated ID for this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxCreateReceiveRequest(const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   tABC_TxDetails *pDetails,
                                   char **pszRequestID,
                                   bool bTransfer,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxAddress *pAddress = NULL;
    tABC_TxDetails *pNewDetails = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(pDetails);
    ABC_CHECK_NULL(pszRequestID);
    *pszRequestID = NULL;

    // Dupe details and default them
    ABC_CHECK_RET(ABC_TxDupDetails(&pNewDetails, pDetails, pError));
    ABC_CHECK_RET(ABC_TxDefaultRequestDetails(szUserName, szPassword, pNewDetails, pError));

    // get a new address (re-using a recycleable if we can)
    ABC_CHECK_RET(ABC_TxCreateNewAddress(szUserName, szPassword, szWalletUUID, pNewDetails, &pAddress, pError));

    // save out this address
    ABC_CHECK_RET(ABC_TxSaveAddress(szUserName, szPassword, szWalletUUID, pAddress, pError));

    // set the id for the caller
    ABC_STRDUP(*pszRequestID, pAddress->szID);

    // Watch this new address
    ABC_CHECK_RET(ABC_TxWatchAddresses(szUserName, szPassword, szWalletUUID, pError));
    ABC_CHECK_RET(
        ABC_BridgeWatchAddr(szUserName, szPassword, szWalletUUID,
                            pAddress->szPubAddress, true, pError));
#if NETWORK_FAKE
    if (!bTransfer)
    {
        ABC_CHECK_RET(
            ABC_TxKickoffFakeReceive(szUserName, szPassword,
                                     szWalletUUID, pAddress->szID, pError));
    }
#endif
exit:
    ABC_TxFreeAddress(pAddress);
    ABC_TxFreeDetails(pNewDetails);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Creates a new address.
 * First looks to see if we can recycle one, if we can, that is the address returned.
 * This new address is not saved to the file system, the caller must make sure it is saved
 * if they want it persisted.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param pDetails      Pointer to transaction details to be used for the new address
 *                      (note: a copy of these are made so the caller can do whatever they want
 *                       with the pointer once the call is complete)
 * @param ppAddress     Location to store pointer to allocated address
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxCreateNewAddress(const char *szUserName,
                               const char *szPassword,
                               const char *szWalletUUID,
                               tABC_TxDetails *pDetails,
                               tABC_TxAddress **ppAddress,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxAddress **aAddresses = NULL;
    unsigned int countAddresses = 0;
    tABC_TxAddress *pAddress = NULL;
    int64_t N = -1;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(pDetails);
    ABC_CHECK_NULL(ppAddress);

    // first look for an existing address that we can re-use

    // load addresses
    ABC_CHECK_RET(ABC_TxGetAddresses(szUserName, szPassword, szWalletUUID, &aAddresses, &countAddresses, pError));

    // search through all of the addresses, get the highest N and check for one with the recycleable bit set
    for (int i = 0; i < countAddresses; i++)
    {
        // if this is the highest seq number
        if (aAddresses[i]->seq > N)
        {
            N = aAddresses[i]->seq;
        }

        // if we don't have an address yet and this one is available
        ABC_CHECK_NULL(aAddresses[i]->pStateInfo);
        if ((pAddress == NULL) && (aAddresses[i]->pStateInfo->bRecycleable == true) && ((aAddresses[i]->pStateInfo->countActivities == 0)))
        {
            pAddress = aAddresses[i];
            // set it to NULL so we don't free it as part of the array free, we will be sending this back to the caller
            aAddresses[i] = NULL;
        }
    }

    // if we found an address, make it ours!
    if (pAddress != NULL)
    {
        // free state and details as we will be setting them to new data below
        ABC_TxFreeAddressStateInfo(pAddress->pStateInfo);
        pAddress->pStateInfo = NULL;
        ABC_FreeTxDetails(pAddress->pDetails);
        pAddress->pDetails = NULL;
    }
    else // if we didn't find an address, we need to create a new one
    {
        ABC_ALLOC(pAddress, sizeof(tABC_TxAddress));

        // get the private seed so we can generate the public address
        tABC_U08Buf Seed = ABC_BUF_NULL;
        ABC_CHECK_RET(ABC_WalletGetBitcoinPrivateSeed(szUserName, szPassword, szWalletUUID, &Seed, pError));

        // generate the public address
        pAddress->szPubAddress = NULL;
        pAddress->seq = (int32_t) N;
        do
        {
            // move to the next sequence number
            pAddress->seq++;

            // Get the public address for our sequence (it can return NULL, if it is invalid)
            ABC_CHECK_RET(ABC_BridgeGetBitcoinPubAddress(&(pAddress->szPubAddress), Seed, pAddress->seq, pError));
        } while (pAddress->szPubAddress == NULL);

        // set the final ID
        ABC_ALLOC(pAddress->szID, TX_MAX_ADDR_ID_LENGTH);
        sprintf(pAddress->szID, "%u", pAddress->seq);
    }

    // add the info we have to this address

    // copy over the info we were given
    ABC_CHECK_RET(ABC_DuplicateTxDetails(&(pAddress->pDetails), pDetails, pError));

    // create the state info
    ABC_ALLOC(pAddress->pStateInfo, sizeof(tTxAddressStateInfo));
    pAddress->pStateInfo->bRecycleable = true;
    pAddress->pStateInfo->countActivities = 0;
    pAddress->pStateInfo->aActivities = NULL;
    pAddress->pStateInfo->timeCreation = time(NULL);

    // assigned final address
    *ppAddress = pAddress;
    pAddress = NULL;

exit:
    ABC_TxFreeAddresses(aAddresses, countAddresses);
    ABC_TxFreeAddress(pAddress);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Modifies a previously created receive request.
 * Note: the previous details will be free'ed so if the user is using the previous details for this request
 * they should not assume they will be valid after this call.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pDetails      Pointer to transaction details
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxModifyReceiveRequest(const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   const char *szRequestID,
                                   tABC_TxDetails *pDetails,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFile = NULL;
    char *szAddrDir = NULL;
    char *szFilename = NULL;
    tABC_TxAddress *pAddress = NULL;
    tABC_TxDetails *pNewDetails = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");
    ABC_CHECK_NULL(pDetails);

    // get the filename for this request (note: interally requests are addresses)
    ABC_CHECK_RET(ABC_GetAddressFilename(szWalletUUID, szRequestID, &szFile, pError));

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // create the full filename
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH + 1);
    sprintf(szFilename, "%s/%s", szAddrDir, szFile);

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(szUserName, szPassword, szWalletUUID, szFilename, &pAddress, pError));

    // copy the new details
    ABC_CHECK_RET(ABC_TxDupDetails(&pNewDetails, pDetails, pError));

    // free the old details on this address
    ABC_TxFreeDetails(pAddress->pDetails);

    // set the new details
    pAddress->pDetails = pNewDetails;
    pNewDetails = NULL;

    // write out the address
    ABC_CHECK_RET(ABC_TxSaveAddress(szUserName, szPassword, szWalletUUID, pAddress, pError));

exit:
    ABC_FREE_STR(szFile);
    ABC_FREE_STR(szAddrDir);
    ABC_FREE_STR(szFilename);
    ABC_TxFreeAddress(pAddress);
    ABC_TxFreeDetails(pNewDetails);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Gets the filename for a given address based upon the address id
 *
 * @param szWalletUUID  UUID of the wallet associated with this address
 * @param szAddressID   ID of this address
 * @param pszFilename   Address to store pointer to filename
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_GetAddressFilename(const char *szWalletUUID,
                               const char *szAddressID,
                               char **pszFilename,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szAddrDir = NULL;
    tABC_FileIOList *pFileList = NULL;

    char *szID = NULL;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError)); // we want this as an atomic files system function
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szAddressID);
    ABC_CHECK_ASSERT(strlen(szAddressID) > 0, ABC_CC_Error, "No address UUID provided");
    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // Make sure there is an addresses directory
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szAddrDir, &bExists, pError));
    ABC_CHECK_ASSERT(bExists == true, ABC_CC_Error, "No existing requests/addresses");

    // get all the files in the address directory
    ABC_FileIOCreateFileList(&pFileList, szAddrDir, NULL);
    for (int i = 0; i < pFileList->nCount; i++)
    {
        // if this file is a normal file
        if (pFileList->apFiles[i]->type == ABC_FileIOFileType_Regular)
        {
            // parse the elements from the filename
            ABC_FREE_STR(szID);
            ABC_CHECK_RET(ABC_TxParseAddrFilename(pFileList->apFiles[i]->szName, &szID, NULL, pError));

            // if the id matches
            if (strcmp(szID, szAddressID) == 0)
            {
                // copy over the filename
                ABC_STRDUP(*pszFilename, pFileList->apFiles[i]->szName);
                break;
            }
        }
    }

exit:
    ABC_FREE_STR(szAddrDir);
    ABC_FREE_STR(szID);
    ABC_FileIOFreeFileList(pFileList);

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

/**
 * Parses out the id and public address from an address filename
 *
 * @param szFilename        Filename to parse
 * @param pszID             Location to store allocated id (caller must free) - optional
 * @param pszPublicAddress  Location to store allocated public address(caller must free) - optional
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxParseAddrFilename(const char *szFilename,
                                char **pszID,
                                char **pszPublicAddress,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    // if the filename is long enough
    if (strlen(szFilename) >= ADDRESS_FILENAME_MIN_LEN)
    {
        int suffixPos = (int) strlen(szFilename) - (int) strlen(ADDRESS_FILENAME_SUFFIX);
        char *szSuffix = (char *) &(szFilename[suffixPos]);

        // if the filename ends with the right suffix
        if (strcmp(ADDRESS_FILENAME_SUFFIX, szSuffix) == 0)
        {
            int nPosSeparator = 0;

            // go through all the characters up to the separator
            for (int i = 0; i < strlen(szFilename); i++)
            {
                // check for id separator
                if (szFilename[i] == ADDRESS_FILENAME_SEPARATOR)
                {
                    // found it
                    nPosSeparator = i;
                    break;
                }

                // if no separator, then better be a digit
                if (isdigit(szFilename[i]) == 0)
                {
                    // Ran into a non-digit! - no good
                    break;
                }
            }

            // if we found a legal separator position
            if (nPosSeparator > 0)
            {
                if (pszID != NULL)
                {
                    ABC_ALLOC(*pszID, nPosSeparator + 1);
                    strncpy(*pszID, szFilename, nPosSeparator);
                }

                if (pszPublicAddress != NULL)
                {
                    ABC_STRDUP(*pszPublicAddress, &(szFilename[nPosSeparator + 1]))
                    (*pszPublicAddress)[strlen(*pszPublicAddress) - strlen(ADDRESS_FILENAME_SUFFIX)] = '\0';
                }
            }
        }
    }

exit:

    return cc;
}

/**
 * Finalizes a previously created receive request.
 * This is done by setting the recycle bit to false so that the address is not used again.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxFinalizeReceiveRequest(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     const char *szRequestID,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");

    // set the recycle bool to false (not that the request is actually an address internally)
    ABC_CHECK_RET(ABC_TxSetAddressRecycle(szUserName, szPassword, szWalletUUID, szRequestID, false, pError));

exit:

    return cc;
}

/**
 * Cancels a previously created receive request.
 * This is done by setting the recycle bit to true so that the address can be used again.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxCancelReceiveRequest(const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   const char *szRequestID,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");

    // set the recycle bool to true (not that the request is actually an address internally)
    ABC_CHECK_RET(ABC_TxSetAddressRecycle(szUserName, szPassword, szWalletUUID, szRequestID, true, pError));

exit:

    return cc;
}

/**
 * Sets the recycle status on an address as specified
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szAddress     ID of the address
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxSetAddressRecycle(const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                const char *szAddress,
                                bool bRecyclable,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFile = NULL;
    char *szAddrDir = NULL;
    char *szFilename = NULL;
    tABC_TxAddress *pAddress = NULL;
    tABC_TxDetails *pNewDetails = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szAddress);
    ABC_CHECK_ASSERT(strlen(szAddress) > 0, ABC_CC_Error, "No address ID provided");

    // get the filename for this address
    ABC_CHECK_RET(ABC_GetAddressFilename(szWalletUUID, szAddress, &szFile, pError));

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // create the full filename
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH + 1);
    sprintf(szFilename, "%s/%s", szAddrDir, szFile);

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(szUserName, szPassword, szWalletUUID, szFilename, &pAddress, pError));
    ABC_CHECK_NULL(pAddress->pStateInfo);

    // if it isn't already set as required
    if (pAddress->pStateInfo->bRecycleable != bRecyclable)
    {
        // change the recycle boolean
        ABC_CHECK_NULL(pAddress->pStateInfo);
        pAddress->pStateInfo->bRecycleable = bRecyclable;

        // write out the address
        ABC_CHECK_RET(ABC_TxSaveAddress(szUserName, szPassword, szWalletUUID, pAddress, pError));
    }

exit:
    ABC_FREE_STR(szFile);
    ABC_FREE_STR(szAddrDir);
    ABC_FREE_STR(szFilename);
    ABC_TxFreeAddress(pAddress);
    ABC_TxFreeDetails(pNewDetails);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Generate the QR code for a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pszURI        Pointer to string to store URI(optional)
 * @param paData        Pointer to store array of data bytes (0x0 white, 0x1 black)
 * @param pWidth        Pointer to store width of image (image will be square)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGenerateRequestQRCode(const char *szUserName,
                                    const char *szPassword,
                                    const char *szWalletUUID,
                                    const char *szRequestID,
                                    char **pszURI,
                                    unsigned char **paData,
                                    unsigned int *pWidth,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxAddress *pAddress = NULL;
    QRcode *qr = NULL;
    unsigned char *aData = NULL;
    unsigned int length = 0;
    char *szURI = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");

    // load the request/address
    ABC_CHECK_RET(ABC_TxLoadAddress(szUserName, szPassword, szWalletUUID, szRequestID, &pAddress, pError));
    ABC_CHECK_NULL(pAddress->pDetails);

    // Get the URL string for this info
    tABC_BitcoinURIInfo infoURI;
    memset(&infoURI, 0, sizeof(tABC_BitcoinURIInfo));
    infoURI.amountSatoshi = pAddress->pDetails->amountSatoshi;
    infoURI.szAddress = pAddress->szPubAddress;
    // if there is a name
    if (pAddress->pDetails->szName)
    {
        if (strlen(pAddress->pDetails->szName) > 0)
        {
            infoURI.szLabel = pAddress->pDetails->szName;
        }
    }

    // if there is a note
    if (pAddress->pDetails->szNotes)
    {
        if (strlen(pAddress->pDetails->szNotes) > 0)
        {
            infoURI.szMessage = pAddress->pDetails->szNotes;
        }
    }
    ABC_CHECK_RET(ABC_BridgeEncodeBitcoinURI(&szURI, &infoURI, pError));

    // encode our string
    ABC_DebugLog("Encoding: %s", szURI);
    qr = QRcode_encodeString(szURI, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    ABC_CHECK_ASSERT(qr != NULL, ABC_CC_Error, "Unable to create QR code");
    length = qr->width * qr->width;
    ABC_ALLOC(aData, length);
    for (int i = 0; i < length; i++)
    {
        aData[i] = qr->data[i] & 0x1;
    }
    *pWidth = qr->width;
    *paData = aData;
    aData = NULL;

    if (pszURI != NULL)
    {
        length = ABC_STRLEN(szURI);
        ABC_ALLOC(*pszURI, length);
        ABC_STRDUP(*pszURI, szURI);
    }

exit:
    ABC_TxFreeAddress(pAddress);
    ABC_FREE_STR(szURI);
    QRcode_free(qr);
    ABC_CLEAR_FREE(aData, length);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Get the specified transactions.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szID              ID of the transaction
 * @param ppTransaction     Location to store allocated transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetTransaction(const char *szUserName,
                             const char *szPassword,
                             const char *szWalletUUID,
                             const char *szID,
                             tABC_TxInfo **ppTransaction,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;
    tABC_TxInfo *pTransaction = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szID);
    ABC_CHECK_ASSERT(strlen(szID) > 0, ABC_CC_Error, "No transaction ID provided");
    ABC_CHECK_NULL(ppTransaction);
    *ppTransaction = NULL;

    // validate the creditials
    ABC_CHECK_RET(ABC_WalletCheckCredentials(szUserName, szPassword, szWalletUUID, pError));

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, szID, true, pError));
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));

    // if the internal doesn't exist
    if (bExists == false)
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, szID, false, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    }

    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

    // load the existing transaction
    ABC_CHECK_RET(ABC_TxLoadTransactionInfo(szUserName, szPassword, szWalletUUID, szFilename, &pTransaction, pError));

    // assign final result
    *ppTransaction = pTransaction;
    pTransaction = NULL;

exit:
    ABC_FREE_STR(szFilename);
    ABC_TxFreeTx(pTx);
    ABC_TxFreeTransaction(pTransaction);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Gets the transactions associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetTransactions(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              tABC_TxInfo ***paTransactions,
                              unsigned int *pCount,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szTxDir = NULL;
    tABC_FileIOList *pFileList = NULL;
    char *szFilename = NULL;
    tABC_TxInfo **aTransactions = NULL;
    unsigned int count = 0;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_RET(ABC_FileIOMutexLock(pError)); // we want this as an atomic files system function
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(paTransactions);
    *paTransactions = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // validate the creditials
    ABC_CHECK_RET(ABC_WalletCheckCredentials(szUserName, szPassword, szWalletUUID, pError));

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetTxDirName(&szTxDir, szWalletUUID, pError));

    // if there is a transaction directory
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szTxDir, &bExists, pError));

    if (bExists == true)
    {
        ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH + 1);

        // get all the files in the transaction directory
        ABC_FileIOCreateFileList(&pFileList, szTxDir, NULL);
        for (int i = 0; i < pFileList->nCount; i++)
        {
            // if this file is a normal file
            if (pFileList->apFiles[i]->type == ABC_FileIOFileType_Regular)
            {
                // create the filename for this transaction
                sprintf(szFilename, "%s/%s", szTxDir, pFileList->apFiles[i]->szName);

                // get the transaction type
                tTxType type = TxType_None;
                ABC_CHECK_RET(ABC_TxGetTxTypeAndBasename(szFilename, &type, NULL, pError));

                // if this is a transaction file (based upon name)
                if (type != TxType_None)
                {
                    bool bHasInternalEquivalent = false;

                    // if this is an external transaction
                    if (type == TxType_External)
                    {
                        // check if it has an internal equivalent and, if so, delete the external
                        ABC_CHECK_RET(ABC_TxCheckForInternalEquivalent(szFilename, &bHasInternalEquivalent, pError));
                    }

                    // if this doesn't not have an internal equivalent (or is an internal itself)
                    if (bHasInternalEquivalent == false)
                    {
                        // add this transaction to the array
                        ABC_CHECK_RET(ABC_TxLoadTxAndAppendToArray(szUserName, szPassword, szWalletUUID, szFilename, &aTransactions, &count, pError));
                    }
                }
            }
        }
    }

    // if we have more than one, then let's sort them
    if (count > 1)
    {
        // sort the transactions by creation date using qsort
        qsort(aTransactions, count, sizeof(tABC_TxInfo *), ABC_TxInfoPtrCompare);
    }

    // store final results
    *paTransactions = aTransactions;
    aTransactions = NULL;
    *pCount = count;
    count = 0;

exit:
    ABC_FREE_STR(szTxDir);
    ABC_FREE_STR(szFilename);
    ABC_FileIOFreeFileList(pFileList);
    ABC_TxFreeTransactions(aTransactions, count);

    ABC_FileIOMutexUnlock(NULL);
    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Searches transactions associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szQuery           Query to search
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxSearchTransactions(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 const char *szQuery,
                                 tABC_TxInfo ***paTransactions,
                                 unsigned int *pCount,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_TxInfo **aTransactions = NULL;
    tABC_TxInfo **aSearchTransactions = NULL;
    unsigned int i;
    unsigned int count = 0;
    unsigned int matchCount = 0;
    char satoshi[15];
    char currency[15];

    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    ABC_CHECK_NULL(paTransactions);
    *paTransactions = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    ABC_TxGetTransactions(szUserName, szPassword, szWalletUUID,
                          &aTransactions, &count, pError);
    ABC_ALLOC(aSearchTransactions, sizeof(tABC_TxInfo*) * count);
    for (i = 0; i < count; i++) {
        memset(satoshi, '\0', 15);
        memset(currency, '\0', 15);
        tABC_TxInfo *pInfo = aTransactions[i];
        snprintf(satoshi, 15, "%ld", pInfo->pDetails->amountSatoshi);
        snprintf(currency, 15, "%f", pInfo->pDetails->amountCurrency);
        if (ABC_TxStrStr(satoshi, szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        }
        if (ABC_TxStrStr(currency, szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        }
        if (ABC_TxStrStr(pInfo->pDetails->szName, szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        }
        else if (ABC_TxStrStr(pInfo->pDetails->szCategory, szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        }
        else if (ABC_TxStrStr(pInfo->pDetails->szNotes, szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        } else {
            ABC_TxFreeTransaction(pInfo);
        }
        aTransactions[i] = NULL;
    }
    if (matchCount > 0)
    {
        ABC_REALLOC(aSearchTransactions, sizeof(tABC_TxInfo *) * matchCount);
    }

    *paTransactions = aSearchTransactions;
    *pCount = matchCount;
    aTransactions = NULL;
exit:
    ABC_FREE(aTransactions);
    return cc;
}

/**
 * Looks to see if a matching internal (i.e., -int) version of this file exists.
 * If it does, this external version is deleted.
 *
 * @param szFilename    Filename of transaction
 * @param pbEquivalent  Pointer to store result
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxCheckForInternalEquivalent(const char *szFilename,
                                         bool *pbEquivalent,
                                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szBasename = NULL;
    char *szFilenameInt = NULL;
    tTxType type = TxType_None;

    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(pbEquivalent);
    *pbEquivalent = false;

    // get the type and the basename of this transaction
    ABC_CHECK_RET(ABC_TxGetTxTypeAndBasename(szFilename, &type, &szBasename, pError));

    // if this is an external
    if (type == TxType_External)
    {
        // create the internal version of the filename
        ABC_ALLOC(szFilenameInt, ABC_FILEIO_MAX_PATH_LENGTH + 1);
        sprintf(szFilenameInt, "%s%s", szBasename, TX_INTERNAL_SUFFIX);

        // check if this internal version of the file exists
        bool bExists = false;
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilenameInt, &bExists, pError));

        // if the internal version exists
        if (bExists)
        {
            // delete the external version (this one)
            ABC_CHECK_RET(ABC_FileIODeleteFile(szFilename, pError));

            *pbEquivalent = true;
        }
    }

exit:
    ABC_FREE_STR(szBasename);
    ABC_FREE_STR(szFilenameInt);

    return cc;
}

/**
 * Given a potential transaction filename, determines the type and
 * creates an allocated basename if it is a transaction type.
 *
 * @param szFilename    Filename of potential transaction
 * @param pType         Pointer to store type
 * @param pszBasename   Pointer to store allocated basename (optional)
 *                      (caller must free)
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxGetTxTypeAndBasename(const char *szFilename,
                                   tTxType *pType,
                                   char **pszBasename,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szBasename = NULL;

    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(pType);

    // assume nothing found
    *pType = TxType_None;
    if (pszBasename != NULL)
    {
        *pszBasename = NULL;
    }

    // look for external the suffix
    int sizeSuffix = strlen(TX_EXTERNAL_SUFFIX);
    if (strlen(szFilename) > sizeSuffix)
    {
        char *szSuffix = (char *) szFilename + (strlen(szFilename) - sizeSuffix);

        // if this file ends with the external suffix
        if (strcmp(szSuffix, TX_EXTERNAL_SUFFIX) == 0)
        {
            *pType = TxType_External;

            // if they want the basename
            if (pszBasename != NULL)
            {
                ABC_STRDUP(szBasename, szFilename);
                szBasename[strlen(szFilename) - sizeSuffix] = '\0';
            }
        }
    }

    // if we haven't found it yet
    if (TxType_None == *pType)
    {
        // check for the internal
        sizeSuffix = strlen(TX_INTERNAL_SUFFIX);
        if (strlen(szFilename) > sizeSuffix)
        {
            char *szSuffix = (char *) szFilename + (strlen(szFilename) - sizeSuffix);

            // if this file ends with the external suffix
            if (strcmp(szSuffix, TX_INTERNAL_SUFFIX) == 0)
            {
                *pType = TxType_Internal;

                // if they want the basename
                if (pszBasename != NULL)
                {
                    ABC_STRDUP(szBasename, szFilename);
                    szBasename[strlen(szFilename) - sizeSuffix] = '\0';
                }
            }
        }
    }

    if (pszBasename != NULL)
    {
        *pszBasename = szBasename;
    }
    szBasename = NULL;

exit:
    ABC_FREE_STR(szBasename);

    return cc;
}

/**
 * Load the specified transaction info.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szFilename        Filename of the transaction
 * @param ppTransaction     Location to store allocated transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadTransactionInfo(const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletUUID,
                                  const char *szFilename,
                                  tABC_TxInfo **ppTransaction,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_Tx *pTx = NULL;
    tABC_TxInfo *pTransaction = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No filename provided");
    ABC_CHECK_NULL(ppTransaction);
    *ppTransaction = NULL;

    // load the transaction
    ABC_CHECK_RET(ABC_TxLoadTransaction(szUserName, szPassword, szWalletUUID, szFilename, &pTx, pError));
    ABC_CHECK_NULL(pTx->pDetails);
    ABC_CHECK_NULL(pTx->pStateInfo);

    // steal the data and assign it to our new struct
    ABC_ALLOC(pTransaction, sizeof(tABC_TxInfo));
    pTransaction->szID = pTx->szID;
    pTx->szID = NULL;
    pTransaction->szMalleableTxId = pTx->pStateInfo->szMalleableTxId;
    pTx->pStateInfo->szMalleableTxId = NULL;
    pTransaction->timeCreation = pTx->pStateInfo->timeCreation;
    pTransaction->pDetails = pTx->pDetails;
    pTx->pDetails = NULL;
    pTransaction->countOutputs = pTx->countOutputs;
    pTx->countOutputs = 0;
    pTransaction->aOutputs = pTx->aOutputs;
    pTx->aOutputs = NULL;

    // assign final result
    *ppTransaction = pTransaction;
    pTransaction = NULL;

exit:
    ABC_TxFreeTx(pTx);
    ABC_TxFreeTransaction(pTransaction);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Loads the given transaction info and adds it to the end of the array
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szFilename        Filename of transaction
 * @param paTransactions    Pointer to array into which the transaction will be added
 * @param pCount            Pointer to store number of transactions (will be updated)
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadTxAndAppendToArray(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     const char *szFilename,
                                     tABC_TxInfo ***paTransactions,
                                     unsigned int *pCount,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxInfo *pTransaction = NULL;
    tABC_TxInfo **aTransactions = NULL;
    unsigned int count = 0;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No transaction filename provided");
    ABC_CHECK_NULL(paTransactions);
    ABC_CHECK_NULL(pCount);

    // hold on to current values
    count = *pCount;
    aTransactions = *paTransactions;

    // load it into the info transaction structure
    ABC_CHECK_RET(ABC_TxLoadTransactionInfo(szUserName, szPassword, szWalletUUID, szFilename, &pTransaction, pError));

    // create space for new entry
    if (aTransactions == NULL)
    {
        ABC_ALLOC(aTransactions, sizeof(tABC_TxInfo *));
        count = 1;
    }
    else
    {
        count++;
        ABC_REALLOC(aTransactions, sizeof(tABC_TxInfo *) * count);
    }

    // add it to the array
    aTransactions[count - 1] = pTransaction;
    pTransaction = NULL;

    // assign the values to the caller
    *paTransactions = aTransactions;
    *pCount = count;

exit:
    ABC_TxFreeTransaction(pTransaction);

    return cc;
}

/**
 * Frees the given transaction
 *
 * @param pTransaction Pointer to transaction to free
 */
void ABC_TxFreeTransaction(tABC_TxInfo *pTransaction)
{
    if (pTransaction)
    {
        ABC_FREE_STR(pTransaction->szID);
        ABC_TxFreeOutputs(pTransaction->aOutputs, pTransaction->countOutputs);
        ABC_TxFreeDetails(pTransaction->pDetails);
        ABC_CLEAR_FREE(pTransaction, sizeof(tABC_TxInfo));
    }
}

/**
 * Frees the given array of transactions
 *
 * @param aTransactions Array of transactions
 * @param count         Number of transactions
 */
void ABC_TxFreeTransactions(tABC_TxInfo **aTransactions,
                            unsigned int count)
{
    if (aTransactions && count > 0)
    {
        for (int i = 0; i < count; i++)
        {
            ABC_TxFreeTransaction(aTransactions[i]);
        }

        ABC_FREE(aTransactions);
    }
}

/**
 * Sets the details for a specific existing transaction.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szID              ID of the transaction
 * @param pDetails          Details for the transaction
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxSetTransactionDetails(const char *szUserName,
                                    const char *szPassword,
                                    const char *szWalletUUID,
                                    const char *szID,
                                    tABC_TxDetails *pDetails,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szID);
    ABC_CHECK_ASSERT(strlen(szID) > 0, ABC_CC_Error, "No transaction ID provided");

    // validate the creditials
    ABC_CHECK_RET(ABC_WalletCheckCredentials(szUserName, szPassword, szWalletUUID, pError));

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, szID, true, pError));
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));

    // if the internal doesn't exist
    if (bExists == false)
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, szID, false, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    }

    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

    // load the existing transaction
    ABC_CHECK_RET(ABC_TxLoadTransaction(szUserName, szPassword, szWalletUUID, szFilename, &pTx, pError));
    ABC_CHECK_NULL(pTx->pDetails);
    ABC_CHECK_NULL(pTx->pStateInfo);

    // modify the details
    pTx->pDetails->amountSatoshi = pDetails->amountSatoshi;
    pTx->pDetails->amountFeesAirbitzSatoshi = pDetails->amountFeesAirbitzSatoshi;
    pTx->pDetails->amountFeesMinersSatoshi = pDetails->amountFeesMinersSatoshi;
    pTx->pDetails->amountCurrency = pDetails->amountCurrency;
    pTx->pDetails->bizId = pDetails->bizId;
    pTx->pDetails->attributes = pDetails->attributes;
    ABC_FREE_STR(pTx->pDetails->szName);
    ABC_STRDUP(pTx->pDetails->szName, pDetails->szName);
    ABC_FREE_STR(pTx->pDetails->szCategory);
    ABC_STRDUP(pTx->pDetails->szCategory, pDetails->szCategory);
    ABC_FREE_STR(pTx->pDetails->szNotes);
    ABC_STRDUP(pTx->pDetails->szNotes, pDetails->szNotes);

    // re-save the transaction
    ABC_CHECK_RET(ABC_TxSaveTransaction(szUserName, szPassword, szWalletUUID, pTx, pError));

exit:
    ABC_FREE_STR(szFilename);
    ABC_TxFreeTx(pTx);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Gets the details for a specific existing transaction.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szID              ID of the transaction
 * @param ppDetails         Location to store allocated details for the transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetTransactionDetails(const char *szUserName,
                                    const char *szPassword,
                                    const char *szWalletUUID,
                                    const char *szID,
                                    tABC_TxDetails **ppDetails,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;
    tABC_TxDetails *pDetails = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szID);
    ABC_CHECK_ASSERT(strlen(szID) > 0, ABC_CC_Error, "No transaction ID provided");
    ABC_CHECK_NULL(ppDetails);

    // validate the creditials
    ABC_CHECK_RET(ABC_WalletCheckCredentials(szUserName, szPassword, szWalletUUID, pError));

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, szID, true, pError));
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));

    // if the internal doesn't exist
    if (bExists == false)
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, szID, false, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    }

    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

    // load the existing transaction
    ABC_CHECK_RET(ABC_TxLoadTransaction(szUserName, szPassword, szWalletUUID, szFilename, &pTx, pError));
    ABC_CHECK_NULL(pTx->pDetails);
    ABC_CHECK_NULL(pTx->pStateInfo);

    // duplicate the details
    ABC_CHECK_RET(ABC_TxDupDetails(&pDetails, pTx->pDetails, pError));

    // assign final result
    *ppDetails = pDetails;
    pDetails = NULL;


exit:
    ABC_FREE_STR(szFilename);
    ABC_TxFreeTx(pTx);
    ABC_TxFreeDetails(pDetails);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Gets the bit coin public address for a specified request
 *
 * @param szUserName        UserName for the account associated with the requests
 * @param szPassword        Password for the account associated with the requests
 * @param szWalletUUID      UUID of the wallet associated with the requests
 * @param szRequestID       ID of request
 * @param pszAddress        Location to store allocated address string (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetRequestAddress(const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                const char *szRequestID,
                                char **pszAddress,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    tABC_TxAddress *pAddress = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");
    ABC_CHECK_NULL(pszAddress);
    *pszAddress = NULL;

    ABC_CHECK_RET(
        ABC_TxLoadAddress(szUserName, szPassword,
                          szWalletUUID, szRequestID,
                          &pAddress, pError));
    ABC_STRDUP(*pszAddress, pAddress->szPubAddress);
exit:
    ABC_TxFreeAddress(pAddress);

    return cc;
}

/**
 * Gets the pending requests associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the requests
 * @param szPassword        Password for the account associated with the requests
 * @param szWalletUUID      UUID of the wallet associated with the requests
 * @param paTransactions    Pointer to store array of requests info pointers
 * @param pCount            Pointer to store number of requests
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetPendingRequests(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 tABC_RequestInfo ***paRequests,
                                 unsigned int *pCount,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxAddress **aAddresses = NULL;
    unsigned int count = 0;
    tABC_RequestInfo **aRequests = NULL;
    unsigned int countPending = 0;
    tABC_RequestInfo *pRequest = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(paRequests);
    *paRequests = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // start by retrieving all address for this wallet
    ABC_CHECK_RET(ABC_TxGetAddresses(szUserName, szPassword, szWalletUUID, &aAddresses, &count, pError));

    // if there are any addresses
    if (count > 0)
    {
        // print out the addresses - debug
        //ABC_TxPrintAddresses(aAddresses, count);

        // walk through all the addresses looking for those with outstanding balances
        for (int i = 0; i < count; i++)
        {
            tABC_TxAddress *pAddr = aAddresses[i];
            ABC_CHECK_NULL(pAddr);
            tABC_TxDetails  *pDetails = pAddr->pDetails;

            // if this address has user details associated with it (i.e., was created by the user)
            if (pDetails)
            {
                tTxAddressStateInfo *pState = pAddr->pStateInfo;
                ABC_CHECK_NULL(pState);

                // if this is not a recyclable address (i.e., it was specifically used for a transaction)
                if (pState->bRecycleable == false)
                {
                    // if this address was used for a request of funds (i.e., not a send)
                    int64_t requestSatoshi = pDetails->amountSatoshi;
                    if (requestSatoshi >= 0)
                    {
                        // get the outstanding balanace on this request/address
                        int64_t owedSatoshi = 0;
                        ABC_CHECK_RET(ABC_TxGetAddressOwed(pAddr, &owedSatoshi, pError));

                        // if money is still owed
                        if (owedSatoshi > 0)
                        {
                            // create this request
                            ABC_ALLOC(pRequest, sizeof(tABC_RequestInfo));
                            ABC_STRDUP(pRequest->szID, pAddr->szID);
                            pRequest->timeCreation = pState->timeCreation;
                            pRequest->owedSatoshi = owedSatoshi;
                            pRequest->amountSatoshi = pDetails->amountSatoshi - owedSatoshi;
                            pRequest->pDetails = pDetails; // steal the info as is
                            pDetails = NULL;
                            pAddr->pDetails = NULL; // we are taking this for our own so later we don't want to free it

                            // increase the array size
                            if (countPending > 0)
                            {
                                countPending++;
                                ABC_REALLOC(aRequests, countPending * sizeof(tABC_RequestInfo *));
                            }
                            else
                            {
                                ABC_ALLOC(aRequests, sizeof(tABC_RequestInfo *));
                                countPending = 1;
                            }

                            // add it to the array
                            aRequests[countPending - 1] = pRequest;
                            pRequest = NULL;
                        }
                    }
                }
            }
        }
    }

    // assign final results
    *paRequests = aRequests;
    aRequests = NULL;
    *pCount = countPending;
    countPending = 0;

exit:
    ABC_TxFreeAddresses(aAddresses, count);
    ABC_TxFreeRequests(aRequests, countPending);
    ABC_TxFreeRequest(pRequest);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Given an address, this function returns the balance remaining on the address
 * It does this by checking the activity amounts against the initial request amount.
 * Negative indicates satoshi is still 'owed' on the address, 'positive' means excess was paid.
 *
 * the big assumption here is that address can be used for making payments after they have been used for
 * receiving payment but those should not be taken into account when determining what has been paid on the
 * address.
 *
 * @param pAddr          Address to check
 * @param pSatoshiOwed   Ptr into which owed amount is stored
 * @param pError         A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxGetAddressOwed(tABC_TxAddress *pAddr,
                             int64_t *pSatoshiOwed,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(pSatoshiOwed);
    *pSatoshiOwed = 0;
    ABC_CHECK_NULL(pAddr);
    tABC_TxDetails  *pDetails = pAddr->pDetails;
    ABC_CHECK_NULL(pDetails);
    tTxAddressStateInfo *pState = pAddr->pStateInfo;
    ABC_CHECK_NULL(pState);

    // start with the amount requested
    int64_t satoshiOwed = pDetails->amountSatoshi;

    // if any activities have occured on this address
    if ((pState->aActivities != NULL) && (pState->countActivities > 0))
    {
        for (int i = 0; i < pState->countActivities; i++)
        {
            // if this activity is money paid on the address
            // note: here is where negative activity is ignored
            // the big assumption here is that address can be used
            // for making payments after they have been used for
            // receiving payment but those should not be taken into
            // account when determining what has been paid on the
            // address
            if (pState->aActivities[i].amountSatoshi > 0)
            {
                // remove that from the request amount
                satoshiOwed -= pState->aActivities[i].amountSatoshi;
            }
        }
    }

    // assign final balance
    *pSatoshiOwed = satoshiOwed;

exit:

    return cc;
}

/**
 * Default the values of request tABC_TxDetails, if they are not already
 * populated. Currently this only populates the szName.
 *
 * @param szUserName
 * @param szPassword
 * @param pError         A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxDefaultRequestDetails(const char *szUserName, const char *szPassword,
                                    tABC_TxDetails *pDetails, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    tABC_AccountSettings *pSettings = NULL;
    tABC_U08Buf Label = ABC_BUF_NULL;

    if (ABC_STRLEN(pDetails->szName) == 0)
    {
        ABC_CHECK_RET(ABC_LoadAccountSettings(szUserName, szPassword, &pSettings, pError));
        if (ABC_STRLEN(pSettings->szFirstName) > 0)
        {
            ABC_BUF_DUP_PTR(Label, pSettings->szFirstName, strlen(pSettings->szFirstName));
        }
        if (ABC_STRLEN(pSettings->szLastName) > 0)
        {
            if (ABC_BUF_PTR(Label) == NULL)
            {
                ABC_BUF_DUP_PTR(Label, pSettings->szLastName, strlen(pSettings->szLastName));
            }
            else
            {
                ABC_BUF_APPEND_PTR(Label, " ", 1);
                ABC_BUF_APPEND_PTR(Label, pSettings->szLastName, strlen(pSettings->szLastName));
            }
        }
        if (ABC_STRLEN(pSettings->szNickname) > 0)
        {
            if (ABC_BUF_PTR(Label) == NULL)
            {
                ABC_BUF_DUP_PTR(Label, pSettings->szNickname, strlen(pSettings->szNickname));
            }
            else
            {
                ABC_BUF_APPEND_PTR(Label, " - ", 3);
                ABC_BUF_APPEND_PTR(Label, pSettings->szNickname, strlen(pSettings->szNickname));
            }
        }
        // Append null byte
        ABC_BUF_APPEND_PTR(Label, "", 1);

        pDetails->szName = (char *)ABC_BUF_PTR(Label);
        ABC_BUF_CLEAR(Label);
    }
exit:
    ABC_BUF_FREE(Label);
    ABC_FreeAccountSettings(pSettings);
    return cc;
}

/**
 * Frees the given requets
 *
 * @param pRequest Ptr to request to free
 */
static
void ABC_TxFreeRequest(tABC_RequestInfo *pRequest)
{
    if (pRequest)
    {
        ABC_TxFreeDetails(pRequest->pDetails);

        ABC_CLEAR_FREE(pRequest, sizeof(tABC_RequestInfo));
    }
}

/**
 * Frees the given array of requets
 *
 * @param aRequests Array of requests
 * @param count     Number of requests
 */
void ABC_TxFreeRequests(tABC_RequestInfo **aRequests,
                        unsigned int count)
{
    if (aRequests && count > 0)
    {
        for (int i = 0; i < count; i++)
        {
            ABC_TxFreeRequest(aRequests[i]);
        }

        ABC_FREE(aRequests);
    }
}

/**
 * Gets the filename for a given transaction
 * format is: N-Base58(HMAC256(TxID,MK)).json
 *
 * @param pszFilename Output filename name. The caller must free this.
 */
static
tABC_CC ABC_TxCreateTxFilename(char **pszFilename, const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szTxID, bool bInternal, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szTxDir = NULL;
    tABC_U08Buf MK = ABC_BUF_NULL;
    tABC_U08Buf DataHMAC = ABC_BUF_NULL;
    char *szDataBase58 = NULL;

    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_NULL(szTxID);

    // Get the master key we will need to encode the filename
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    ABC_CHECK_RET(ABC_WalletGetTxDirName(&szTxDir, szWalletUUID, pError));

    // create an hmac-256 of the TxID
    tABC_U08Buf TxID = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(TxID, (unsigned char *)szTxID, strlen(szTxID));
    ABC_CHECK_RET(ABC_CryptoHMAC256(TxID, MK, &DataHMAC, pError));

    // create a base58 of the hmac-256 TxID
    ABC_CHECK_RET(ABC_CryptoBase58Encode(DataHMAC, &szDataBase58, pError));

    ABC_ALLOC(*pszFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszFilename, "%s/%s%s", szTxDir, szDataBase58, (bInternal ? TX_INTERNAL_SUFFIX : TX_EXTERNAL_SUFFIX));

exit:
    ABC_FREE_STR(szTxDir);
    ABC_BUF_FREE(DataHMAC);
    ABC_FREE_STR(szDataBase58);

    return cc;
}

/**
 * Loads a transaction from disk
 *
 * @param ppTx  Pointer to location to hold allocated transaction
 *              (it is the callers responsiblity to free this transaction)
 */
static
tABC_CC ABC_TxLoadTransaction(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              const char *szFilename,
                              tABC_Tx **ppTx,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf MK = ABC_BUF_NULL;
    json_t *pJSON_Root = NULL;
    tABC_Tx *pTx = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No filename provided");
    ABC_CHECK_NULL(ppTx);
    *ppTx = NULL;

    // Get the master key we will need to decode the transaction data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    // make sure the transaction exists
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

    // load the json object (load file, decrypt it, create json object
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, MK, &pJSON_Root, pError));

    // start decoding

    ABC_ALLOC(pTx, sizeof(tABC_Tx));

    // get the id
    json_t *jsonVal = json_object_get(pJSON_Root, JSON_TX_ID_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing id");
    ABC_STRDUP(pTx->szID, json_string_value(jsonVal));

    // get the state object
    ABC_CHECK_RET(ABC_TxDecodeTxState(pJSON_Root, &(pTx->pStateInfo), pError));

    // get the details object
    ABC_CHECK_RET(ABC_TxDecodeTxDetails(pJSON_Root, &(pTx->pDetails), pError));

    // get the addresses array (if it exists)
    json_t *jsonOutputs = json_object_get(pJSON_Root, JSON_TX_OUTPUTS_FIELD);
    if (jsonOutputs)
    {
        ABC_CHECK_ASSERT(json_is_array(jsonOutputs), ABC_CC_JSONError, "Error parsing JSON transaction package - missing addresses array");

        // get the number of elements in the array
        pTx->countOutputs = (int) json_array_size(jsonOutputs);

        if (pTx->countOutputs > 0)
        {
            ABC_ALLOC(pTx->aOutputs, sizeof(tABC_TxOutput *) * pTx->countOutputs);

            for (int i = 0; i < pTx->countOutputs; i++)
            {
                ABC_ALLOC(pTx->aOutputs[i], sizeof(tABC_TxOutput));

                json_t *pJSON_Elem = json_array_get(jsonOutputs, i);
                ABC_CHECK_ASSERT((pJSON_Elem && json_is_object(pJSON_Elem)), ABC_CC_JSONError, "Error parsing JSON transaction output - missing object");

                json_t *jsonVal = json_object_get(pJSON_Elem, JSON_TX_OUTPUT_FLAG);
                ABC_CHECK_ASSERT((jsonVal && json_is_boolean(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction output - missing input boolean");
                pTx->aOutputs[i]->input = json_is_true(jsonVal) ? true : false;

                jsonVal = json_object_get(pJSON_Elem, JSON_TX_OUTPUT_VALUE);
                ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing address array element");
                pTx->aOutputs[i]->value = json_integer_value(jsonVal);

                jsonVal = json_object_get(pJSON_Elem, JSON_TX_OUTPUT_ADDRESS);
                ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing address array element");
                ABC_STRDUP(pTx->aOutputs[i]->szAddress, json_string_value(jsonVal));

                jsonVal = json_object_get(pJSON_Elem, JSON_TX_OUTPUT_TXID);
                if (jsonVal)
                {
                    ABC_CHECK_ASSERT(json_is_string(jsonVal), ABC_CC_JSONError, "Error parsing JSON transaction package - missing txid");
                    ABC_STRDUP(pTx->aOutputs[i]->szTxId, json_string_value(jsonVal));
                }

                jsonVal = json_object_get(pJSON_Elem, JSON_TX_OUTPUT_INDEX);
                if (jsonVal)
                {
                    ABC_CHECK_ASSERT(json_is_integer(jsonVal), ABC_CC_JSONError, "Error parsing JSON transaction package - missing index");
                    pTx->aOutputs[i]->index = json_integer_value(jsonVal);
                }
            }
        }
    }
    else
    {
        pTx->countOutputs = 0;
    }

    // assign final result
    *ppTx = pTx;
    pTx = NULL;

exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    ABC_TxFreeTx(pTx);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Retrieve an Address by the public address
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szMatchAddress    The public address to find a match against
 * @param ppMatched         A pointer to store the matched address to (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
static tABC_CC ABC_TxFindRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 const char *szMatchAddress,
                                 tABC_TxAddress **ppMatched,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_TxAddress **aAddresses = NULL;
    tABC_TxAddress *pMatched = NULL;
    unsigned int countAddresses = 0;

    ABC_CHECK_NULL(ppMatched);

    ABC_CHECK_RET(
        ABC_TxGetAddresses(szUserName, szPassword, szWalletUUID,
                           &aAddresses, &countAddresses, pError));
    for (int i = 0; i < countAddresses; i++)
    {
        if (strncmp(aAddresses[i]->szPubAddress, szMatchAddress,
                    strlen(aAddresses[i]->szPubAddress)) == 0) {
            ABC_CHECK_RET(ABC_TxLoadAddress(szUserName, szPassword,
                                            szWalletUUID, aAddresses[i]->szID,
                                            &pMatched, pError));
            break;
        }
    }
    *ppMatched = pMatched;
exit:
    ABC_TxFreeAddresses(aAddresses, countAddresses);
    return cc;
}

/**
 * Decodes the transaction state data from a json transaction object
 *
 * @param ppInfo Pointer to store allocated state info
 *               (it is the callers responsiblity to free this)
 */
static
tABC_CC ABC_TxDecodeTxState(json_t *pJSON_Obj, tTxStateInfo **ppInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tTxStateInfo *pInfo = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppInfo);
    *ppInfo = NULL;

    // allocate the struct
    ABC_ALLOC(pInfo, sizeof(tTxStateInfo));

    // get the state object
    json_t *jsonState = json_object_get(pJSON_Obj, JSON_TX_STATE_FIELD);
    ABC_CHECK_ASSERT((jsonState && json_is_object(jsonState)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing state");

    // get the creation date
    json_t *jsonVal = json_object_get(jsonState, JSON_CREATION_DATE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing creation date");
    pInfo->timeCreation = json_integer_value(jsonVal);

    jsonVal = json_object_get(jsonState, JSON_MALLEABLE_TX_ID);
    if (jsonVal)
    {
        ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing malleable tx id");
        ABC_STRDUP(pInfo->szMalleableTxId, json_string_value(jsonVal));
    }

    // get the internal boolean
    jsonVal = json_object_get(jsonState, JSON_TX_INTERNAL_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_boolean(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing internal boolean");
    pInfo->bInternal = json_is_true(jsonVal) ? true : false;

    // assign final result
    *ppInfo = pInfo;
    pInfo = NULL;

exit:
    ABC_CLEAR_FREE(pInfo, sizeof(tTxStateInfo));

    return cc;
}

/**
 * Decodes the transaction details data from a json transaction or address object
 *
 * @param ppInfo Pointer to store allocated meta info
 *               (it is the callers responsiblity to free this)
 */
static
tABC_CC ABC_TxDecodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails **ppDetails, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxDetails *pDetails = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppDetails);
    *ppDetails = NULL;

    // allocated the struct
    ABC_ALLOC(pDetails, sizeof(tABC_TxDetails));

    // get the details object
    json_t *jsonDetails = json_object_get(pJSON_Obj, JSON_DETAILS_FIELD);
    ABC_CHECK_ASSERT((jsonDetails && json_is_object(jsonDetails)), ABC_CC_JSONError, "Error parsing JSON details package - missing meta data (details)");

    // get the satoshi field
    json_t *jsonVal = json_object_get(jsonDetails, JSON_AMOUNT_SATOSHI_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing satoshi amount");
    pDetails->amountSatoshi = json_integer_value(jsonVal);

    // get the airbitz fees satoshi field
    jsonVal = json_object_get(jsonDetails, JSON_AMOUNT_AIRBITZ_FEE_SATOSHI_FIELD);
    if (jsonVal)
    {
        ABC_CHECK_ASSERT(json_is_integer(jsonVal), ABC_CC_JSONError, "Error parsing JSON details package - malformed airbitz fees field");
        pDetails->amountFeesAirbitzSatoshi = json_integer_value(jsonVal);
    }

    // get the miners fees satoshi field
    jsonVal = json_object_get(jsonDetails, JSON_AMOUNT_MINERS_FEE_SATOSHI_FIELD);
    if (jsonVal)
    {
        ABC_CHECK_ASSERT(json_is_integer(jsonVal), ABC_CC_JSONError, "Error parsing JSON details package - malformed miners fees field");
        pDetails->amountFeesMinersSatoshi = json_integer_value(jsonVal);
    }

    // get the currency field
    jsonVal = json_object_get(jsonDetails, JSON_TX_AMOUNT_CURRENCY_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_real(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing currency amount");
    pDetails->amountCurrency = json_real_value(jsonVal);

    // get the name field
    jsonVal = json_object_get(jsonDetails, JSON_TX_NAME_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing name");
    ABC_STRDUP(pDetails->szName, json_string_value(jsonVal));

    // get the business-directory id field
    jsonVal = json_object_get(jsonDetails, JSON_TX_BIZID_FIELD);
    if (jsonVal)
    {
        ABC_CHECK_ASSERT(json_is_integer(jsonVal), ABC_CC_JSONError, "Error parsing JSON details package - malformed directory bizId field");
        pDetails->bizId = json_integer_value(jsonVal);
    }

    // get the category field
    jsonVal = json_object_get(jsonDetails, JSON_TX_CATEGORY_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing category");
    ABC_STRDUP(pDetails->szCategory, json_string_value(jsonVal));

    // get the notes field
    jsonVal = json_object_get(jsonDetails, JSON_TX_NOTES_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing notes");
    ABC_STRDUP(pDetails->szNotes, json_string_value(jsonVal));

    // get the attributes field
    jsonVal = json_object_get(jsonDetails, JSON_TX_ATTRIBUTES_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing attributes");
    pDetails->attributes = (unsigned int) json_integer_value(jsonVal);

    // assign final result
    *ppDetails = pDetails;
    pDetails = NULL;

exit:
    ABC_TxFreeDetails(pDetails);

    return cc;
}

/**
 * Free's a tABC_Tx struct and all its elements
 */
static
void ABC_TxFreeTx(tABC_Tx *pTx)
{
    if (pTx)
    {
        ABC_FREE_STR(pTx->szID);
        ABC_TxFreeDetails(pTx->pDetails);
        ABC_CLEAR_FREE(pTx->pStateInfo, sizeof(tTxStateInfo));
        ABC_TxFreeOutputs(pTx->aOutputs, pTx->countOutputs);
        ABC_CLEAR_FREE(pTx, sizeof(tABC_Tx));
    }
}

/**
 * Creates the transaction directory if needed
 */
static
tABC_CC ABC_TxCreateTxDir(const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szTxDir = NULL;

    // get the transaction directory
    ABC_CHECK_RET(ABC_WalletGetTxDirName(&szTxDir, szWalletUUID, pError));

    // if transaction dir doesn't exist, create it
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szTxDir, &bExists, pError));
    if (true != bExists)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szTxDir, pError));
    }

exit:
    ABC_FREE_STR(szTxDir);

    return cc;
}

/**
 * Saves a transaction to disk
 *
 * @param pTx  Pointer to transaction data
 */
static
tABC_CC ABC_TxSaveTransaction(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              const tABC_Tx *pTx,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf MK = ABC_BUF_NULL;
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;
    json_t *pJSON_OutputArray = NULL;
    json_t **ppJSON_Output = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(pTx);
    ABC_CHECK_NULL(pTx->szID);
    ABC_CHECK_ASSERT(strlen(pTx->szID) > 0, ABC_CC_Error, "No transaction ID provided");
    ABC_CHECK_NULL(pTx->pStateInfo);

    // Get the master key we will need to encode the transaction data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    // create the json for the transaction
    pJSON_Root = json_object();
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_Error, "Could not create transaction JSON object");

    // set the ID
    json_object_set_new(pJSON_Root, JSON_TX_ID_FIELD, json_string(pTx->szID));

    // set the state info
    ABC_CHECK_RET(ABC_TxEncodeTxState(pJSON_Root, pTx->pStateInfo, pError));

    // set the details
    ABC_CHECK_RET(ABC_TxEncodeTxDetails(pJSON_Root, pTx->pDetails, pError));

    // create the addresses array object
    pJSON_OutputArray = json_array();

    // if there are any addresses
    if ((pTx->countOutputs > 0) && (pTx->aOutputs != NULL))
    {
        ABC_ALLOC(ppJSON_Output, sizeof(json_t *) * pTx->countOutputs);
        for (int i = 0; i < pTx->countOutputs; i++)
        {
            ppJSON_Output[i] = json_object();

            int retVal = json_object_set_new(ppJSON_Output[i], JSON_TX_OUTPUT_FLAG, json_boolean(pTx->aOutputs[i]->input));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            retVal = json_object_set_new(ppJSON_Output[i], JSON_TX_OUTPUT_VALUE, json_integer(pTx->aOutputs[i]->value));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            retVal = json_object_set_new(ppJSON_Output[i], JSON_TX_OUTPUT_ADDRESS, json_string(pTx->aOutputs[i]->szAddress));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            retVal = json_object_set_new(ppJSON_Output[i], JSON_TX_OUTPUT_TXID, json_string(pTx->aOutputs[i]->szTxId));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            retVal = json_object_set_new(ppJSON_Output[i], JSON_TX_OUTPUT_INDEX, json_integer(pTx->aOutputs[i]->index));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            // add output to the array
            retVal = json_array_append_new(pJSON_OutputArray, ppJSON_Output[i]);
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");
        }
    }

    // add the address array to the  object
    int retVal = json_object_set(pJSON_Root, JSON_TX_OUTPUTS_FIELD, pJSON_OutputArray);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // create the transaction directory if needed
    ABC_CHECK_RET(ABC_TxCreateTxDir(szWalletUUID, pError));

    // get the filename for this transaction
    ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, pTx->szID, pTx->pStateInfo->bInternal, pError));

    // save out the transaction object to a file encrypted with the master key
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    ABC_FREE_STR(szFilename);
    ABC_CLEAR_FREE(ppJSON_Output, sizeof(json_t *) * pTx->countOutputs);
    if (pJSON_Root) json_decref(pJSON_Root);
    if (pJSON_OutputArray) json_decref(pJSON_OutputArray);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Encodes the transaction state data into the given json transaction object
 *
 * @param pJSON_Obj Pointer to the json object into which the state data is stored.
 * @param pInfo     Pointer to the state data to store in the json object.
 */
static
tABC_CC ABC_TxEncodeTxState(json_t *pJSON_Obj, tTxStateInfo *pInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    json_t *pJSON_State = NULL;
    int retVal = 0;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(pInfo);

    // create the state object
    pJSON_State = json_object();

    // add the creation date to the state object
    retVal = json_object_set_new(pJSON_State, JSON_CREATION_DATE_FIELD, json_integer(pInfo->timeCreation));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the creation date to the state object
    retVal = json_object_set_new(pJSON_State, JSON_MALLEABLE_TX_ID, json_string(pInfo->szMalleableTxId));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the internal boolean (internally created or created due to bitcoin event)
    retVal = json_object_set_new(pJSON_State, JSON_TX_INTERNAL_FIELD, json_boolean(pInfo->bInternal));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the state object to the master object
    retVal = json_object_set(pJSON_Obj, JSON_TX_STATE_FIELD, pJSON_State);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

exit:
    if (pJSON_State) json_decref(pJSON_State);

    return cc;
}

/**
 * Encodes the transaction details data into the given json transaction object
 *
 * @param pJSON_Obj Pointer to the json object into which the details are stored.
 * @param pDetails  Pointer to the details to store in the json object.
 */
static
tABC_CC ABC_TxEncodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails *pDetails, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    json_t *pJSON_Details = NULL;
    int retVal = 0;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(pDetails);

    // create the details object
    pJSON_Details = json_object();

    // add the satoshi field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_AMOUNT_SATOSHI_FIELD, json_integer(pDetails->amountSatoshi));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the airbitz fees satoshi field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_AMOUNT_AIRBITZ_FEE_SATOSHI_FIELD, json_integer(pDetails->amountFeesAirbitzSatoshi));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the miners fees satoshi field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_AMOUNT_MINERS_FEE_SATOSHI_FIELD, json_integer(pDetails->amountFeesMinersSatoshi));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the currency field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_AMOUNT_CURRENCY_FIELD, json_real(pDetails->amountCurrency));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the name field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_NAME_FIELD, json_string(pDetails->szName));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the business-directory id field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_BIZID_FIELD, json_integer(pDetails->bizId));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the category field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_CATEGORY_FIELD, json_string(pDetails->szCategory));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the notes field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_NOTES_FIELD, json_string(pDetails->szNotes));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the attributes field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_ATTRIBUTES_FIELD, json_integer(pDetails->attributes));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the details object to the master object
    retVal = json_object_set(pJSON_Obj, JSON_DETAILS_FIELD, pJSON_Details);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

exit:
    if (pJSON_Details) json_decref(pJSON_Details);

    return cc;
}

/**
 * This function is used to support sorting an array of tTxInfo pointers via qsort.
 * qsort has the following documentation for the required function:
 *
 * Pointer to a function that compares two elements.
 * This function is called repeatedly by qsort to compare two elements. It shall follow the following prototype:
 *
 * int compar (const void* p1, const void* p2);
 *
 * Taking two pointers as arguments (both converted to const void*). The function defines the order of the elements by returning (in a stable and transitive manner):
 * return value	meaning
 * <0	The element pointed by p1 goes before the element pointed by p2
 * 0	The element pointed by p1 is equivalent to the element pointed by p2
 * >0	The element pointed by p1 goes after the element pointed by p2
 *
 */
static
int ABC_TxInfoPtrCompare (const void * a, const void * b)
{
    tABC_TxInfo **ppInfoA = (tABC_TxInfo **)a;
    tABC_TxInfo *pInfoA = (tABC_TxInfo *)*ppInfoA;
    tABC_TxInfo **ppInfoB = (tABC_TxInfo **)b;
    tABC_TxInfo *pInfoB = (tABC_TxInfo *)*ppInfoB;

    if (pInfoA->timeCreation < pInfoB->timeCreation) return -1;
    if (pInfoA->timeCreation == pInfoB->timeCreation) return 0;
    if (pInfoA->timeCreation > pInfoB->timeCreation) return 1;

    return 0;
}

/**
 * Sets the recycle status on an address as specified
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szAddressID   ID of the address
 * @param ppAddress     Pointer to location to store allocated address info
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadAddress(const char *szUserName,
                          const char *szPassword,
                          const char *szWalletUUID,
                          const char *szAddressID,
                          tABC_TxAddress **ppAddress,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFile = NULL;
    char *szAddrDir = NULL;
    char *szFilename = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szAddressID);
    ABC_CHECK_ASSERT(strlen(szAddressID) > 0, ABC_CC_Error, "No address ID provided");
    ABC_CHECK_NULL(ppAddress);

    // get the filename for this address
    ABC_CHECK_RET(ABC_GetAddressFilename(szWalletUUID, szAddressID, &szFile, pError));

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // create the full filename
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH + 1);
    sprintf(szFilename, "%s/%s", szAddrDir, szFile);

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(szUserName, szPassword, szWalletUUID, szFilename, ppAddress, pError));

exit:
    ABC_FREE_STR(szFile);
    ABC_FREE_STR(szAddrDir);
    ABC_FREE_STR(szFilename);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Loads an address from disk given filename (complete path)
 *
 * @param ppAddress  Pointer to location to hold allocated address
 *                   (it is the callers responsiblity to free this address)
 */
static
tABC_CC ABC_TxLoadAddressFile(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              const char *szFilename,
                              tABC_TxAddress **ppAddress,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf MK = ABC_BUF_NULL;
    json_t *pJSON_Root = NULL;
    tABC_TxAddress *pAddress = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No filename provided");
    ABC_CHECK_NULL(ppAddress);
    *ppAddress = NULL;

    // Get the master key we will need to decode the transaction data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    // make sure the addresss exists
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoRequest, "Request address does not exist");

    // load the json object (load file, decrypt it, create json object
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, MK, &pJSON_Root, pError));

    // start decoding

    ABC_ALLOC(pAddress, sizeof(tABC_TxAddress));

    // get the seq and id
    json_t *jsonVal = json_object_get(pJSON_Root, JSON_ADDR_SEQ_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing seq");
    pAddress->seq = (uint32_t)json_integer_value(jsonVal);
    ABC_ALLOC(pAddress->szID, TX_MAX_ADDR_ID_LENGTH);
    sprintf(pAddress->szID, "%u", pAddress->seq);

    // get the public address field
    jsonVal = json_object_get(pJSON_Root, JSON_ADDR_ADDRESS_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing address");
    ABC_STRDUP(pAddress->szPubAddress, json_string_value(jsonVal));

    // get the state object
    ABC_CHECK_RET(ABC_TxDecodeAddressStateInfo(pJSON_Root, &(pAddress->pStateInfo), pError));

    // get the details object
    ABC_CHECK_RET(ABC_TxDecodeTxDetails(pJSON_Root, &(pAddress->pDetails), pError));

    // assign final result
    *ppAddress = pAddress;
    pAddress = NULL;

exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    ABC_TxFreeAddress(pAddress);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Decodes the address state info from a json address object
 *
 * @param ppState Pointer to store allocated state info
 *               (it is the callers responsiblity to free this)
 */
static
tABC_CC ABC_TxDecodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo **ppState, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tTxAddressStateInfo *pState = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppState);
    *ppState = NULL;

    // allocated the struct
    ABC_ALLOC(pState, sizeof(tTxAddressStateInfo));

    // get the state object
    json_t *jsonState = json_object_get(pJSON_Obj, JSON_ADDR_STATE_FIELD);
    ABC_CHECK_ASSERT((jsonState && json_is_object(jsonState)), ABC_CC_JSONError, "Error parsing JSON address package - missing state info");

    // get the creation date
    json_t *jsonVal = json_object_get(jsonState, JSON_CREATION_DATE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing creation date");
    pState->timeCreation = json_integer_value(jsonVal);

    // get the internal boolean
    jsonVal = json_object_get(jsonState, JSON_ADDR_RECYCLEABLE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_boolean(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing recycleable boolean");
    pState->bRecycleable = json_is_true(jsonVal) ? true : false;

    // get the activity array (if it exists)
    json_t *jsonActivity = json_object_get(jsonState, JSON_ADDR_ACTIVITY_FIELD);
    if (jsonActivity)
    {
        ABC_CHECK_ASSERT(json_is_array(jsonActivity), ABC_CC_JSONError, "Error parsing JSON address package - missing activity array");

        // get the number of elements in the array
        pState->countActivities = (int) json_array_size(jsonActivity);

        if (pState->countActivities > 0)
        {
            ABC_ALLOC(pState->aActivities, sizeof(tTxAddressActivity) * pState->countActivities);

            for (int i = 0; i < pState->countActivities; i++)
            {
                json_t *pJSON_Elem = json_array_get(jsonActivity, i);
                ABC_CHECK_ASSERT((pJSON_Elem && json_is_object(pJSON_Elem)), ABC_CC_JSONError, "Error parsing JSON address package - missing activity array element");

                // get the tx id
                jsonVal = json_object_get(pJSON_Elem, JSON_TX_ID_FIELD);
                ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing activity txid");
                ABC_STRDUP(pState->aActivities[i].szTxID, json_string_value(jsonVal));

                // get the date field
                jsonVal = json_object_get(pJSON_Elem, JSON_ADDR_DATE_FIELD);
                ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing date");
                pState->aActivities[i].timeCreation = json_integer_value(jsonVal);

                // get the satoshi field
                jsonVal = json_object_get(pJSON_Elem, JSON_AMOUNT_SATOSHI_FIELD);
                ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing satoshi amount");
                pState->aActivities[i].amountSatoshi = json_integer_value(jsonVal);
            }
        }
    }
    else
    {
        pState->countActivities = 0;
    }

    // assign final result
    *ppState = pState;
    pState = NULL;

exit:
    ABC_TxFreeAddressStateInfo(pState);

    return cc;
}

/**
 * Saves an address to disk
 *
 * @param pAddress  Pointer to address data
 */
static
tABC_CC ABC_TxSaveAddress(const char *szUserName,
                          const char *szPassword,
                          const char *szWalletUUID,
                          const tABC_TxAddress *pAddress,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf MK = ABC_BUF_NULL;
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(pAddress);
    ABC_CHECK_NULL(pAddress->szID);
    ABC_CHECK_ASSERT(strlen(pAddress->szID) > 0, ABC_CC_Error, "No address ID provided");
    ABC_CHECK_NULL(pAddress->pStateInfo);

    // Get the master key we will need to encode the address data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    // create the json for the transaction
    pJSON_Root = json_object();
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_Error, "Could not create address JSON object");

    // set the seq
    json_object_set_new(pJSON_Root, JSON_ADDR_SEQ_FIELD, json_integer(pAddress->seq));

    // set the address
    json_object_set_new(pJSON_Root, JSON_ADDR_ADDRESS_FIELD, json_string(pAddress->szPubAddress));

    // set the state info
    ABC_CHECK_RET(ABC_TxEncodeAddressStateInfo(pJSON_Root, pAddress->pStateInfo, pError));

    // set the details
    ABC_CHECK_RET(ABC_TxEncodeTxDetails(pJSON_Root, pAddress->pDetails, pError));

    // create the address directory if needed
    ABC_CHECK_RET(ABC_TxCreateAddressDir(szWalletUUID, pError));

    // create the filename for this transaction
    ABC_CHECK_RET(ABC_TxCreateAddressFilename(&szFilename, szUserName, szPassword, szWalletUUID, pAddress, pError));

    // save out the transaction object to a file encrypted with the master key
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    ABC_FREE_STR(szFilename);
    if (pJSON_Root) json_decref(pJSON_Root);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Encodes the address state data into the given json transaction object
 *
 * @param pJSON_Obj Pointer to the json object into which the state info is stored.
 * @param pInfo     Pointer to the state info to store in the json object.
 */
static
tABC_CC ABC_TxEncodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo *pInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    json_t *pJSON_State = NULL;
    json_t *pJSON_ActivityArray = NULL;
    json_t *pJSON_Activity = NULL;
    int retVal = 0;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(pInfo);

    // create the state object
    pJSON_State = json_object();

    // add the creation date to the state object
    retVal = json_object_set_new(pJSON_State, JSON_CREATION_DATE_FIELD, json_integer(pInfo->timeCreation));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the recycleable boolean
    retVal = json_object_set_new(pJSON_State, JSON_ADDR_RECYCLEABLE_FIELD, json_boolean(pInfo->bRecycleable));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // create the array object
    pJSON_ActivityArray = json_array();

    // if there are any activities
    if ((pInfo->countActivities > 0) && (pInfo->aActivities != NULL))
    {
        for (int i = 0; i < pInfo->countActivities; i++)
        {
            // create the array object
            pJSON_Activity = json_object();

            // add the ntxid to the activity object
            retVal = json_object_set_new(pJSON_Activity, JSON_TX_ID_FIELD, json_string(pInfo->aActivities[i].szTxID));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            // add the date to the activity object
            retVal = json_object_set_new(pJSON_Activity, JSON_ADDR_DATE_FIELD, json_integer(pInfo->aActivities[i].timeCreation));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            // add the amount satoshi to the activity object
            retVal = json_object_set_new(pJSON_Activity, JSON_AMOUNT_SATOSHI_FIELD, json_integer(pInfo->aActivities[i].amountSatoshi));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            // add this activity to the activity array
            json_array_append_new(pJSON_ActivityArray, pJSON_Activity);

            // the appent_new stole the reference so we are done with it
            pJSON_Activity = NULL;
        }
    }

    // add the activity array to the state object
    retVal = json_object_set(pJSON_State, JSON_ADDR_ACTIVITY_FIELD, pJSON_ActivityArray);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the state object to the master object
    retVal = json_object_set(pJSON_Obj, JSON_ADDR_STATE_FIELD, pJSON_State);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

exit:
    if (pJSON_State) json_decref(pJSON_State);
    if (pJSON_ActivityArray) json_decref(pJSON_ActivityArray);
    if (pJSON_Activity) json_decref(pJSON_Activity);

    return cc;
}

/**
 * Gets the filename for a given address
 * format is: N-Base58(HMAC256(pub_address,MK)).json
 *
 * @param pszFilename Output filename name. The caller must free this.
 */
static
tABC_CC ABC_TxCreateAddressFilename(char **pszFilename, const char *szUserName, const char *szPassword, const char *szWalletUUID, const tABC_TxAddress *pAddress, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szAddrDir = NULL;
    tABC_U08Buf MK = ABC_BUF_NULL;
    tABC_U08Buf DataHMAC = ABC_BUF_NULL;
    char *szDataBase58 = NULL;

    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_NULL(pAddress);

    // Get the master key we will need to encode the filename
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // create an hmac-256 of the public address
    tABC_U08Buf PubAddress = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(PubAddress, (unsigned char *)pAddress->szPubAddress, strlen(pAddress->szPubAddress));
    ABC_CHECK_RET(ABC_CryptoHMAC256(PubAddress, MK, &DataHMAC, pError));

    // create a base58 of the hmac-256 public address
    ABC_CHECK_RET(ABC_CryptoBase58Encode(DataHMAC, &szDataBase58, pError));

    // create the filename
    ABC_ALLOC(*pszFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszFilename, "%s/%u-%s.json", szAddrDir, pAddress->seq, szDataBase58);

exit:
    ABC_FREE_STR(szAddrDir);
    ABC_BUF_FREE(DataHMAC);
    ABC_FREE_STR(szDataBase58);

    return cc;
}

/**
 * Creates the address directory if needed
 */
static
tABC_CC ABC_TxCreateAddressDir(const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szAddrDir = NULL;

    // get the address directory
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // if transaction dir doesn't exist, create it
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szAddrDir, &bExists, pError));
    if (true != bExists)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szAddrDir, pError));
    }

exit:
    ABC_FREE_STR(szAddrDir);

    return cc;
}

/**
 * Free's a ABC_TxFreeAddress struct and all its elements
 */
static
void ABC_TxFreeAddress(tABC_TxAddress *pAddress)
{
    if (pAddress)
    {
        ABC_FREE_STR(pAddress->szID);
        ABC_FREE_STR(pAddress->szPubAddress);
        ABC_TxFreeDetails(pAddress->pDetails);
        ABC_TxFreeAddressStateInfo(pAddress->pStateInfo);

        ABC_CLEAR_FREE(pAddress, sizeof(tABC_TxAddress));
    }
}

/**
 * Free's a tTxAddressStateInfo struct and all its elements
 */
static
void ABC_TxFreeAddressStateInfo(tTxAddressStateInfo *pInfo)
{
    if (pInfo)
    {
        if ((pInfo->aActivities != NULL) && (pInfo->countActivities > 0))
        {
            for (int i = 0; i < pInfo->countActivities; i++)
            {
                ABC_FREE_STR(pInfo->aActivities[i].szTxID);
            }
            ABC_CLEAR_FREE(pInfo->aActivities, sizeof(tTxAddressActivity) * pInfo->countActivities);
        }

        ABC_CLEAR_FREE(pInfo, sizeof(tTxAddressStateInfo));
    }
}

/**
 * Free's an array of  ABC_TxFreeAddress structs
 */
static
void ABC_TxFreeAddresses(tABC_TxAddress **aAddresses, unsigned int count)
{
    if ((aAddresses != NULL) && (count > 0))
    {
        for (int i = 0; i < count; i++)
        {
            ABC_TxFreeAddress(aAddresses[i]);
        }

        ABC_CLEAR_FREE(aAddresses, sizeof(tABC_TxAddress *) * count);
    }
}

static
void ABC_TxFreeOutput(tABC_TxOutput *pOutput)
{
    if (pOutput)
    {
        ABC_FREE_STR(pOutput->szAddress);
        ABC_FREE_STR(pOutput->szTxId);
        ABC_CLEAR_FREE(pOutput, sizeof(tABC_TxOutput));
    }
}

static void
ABC_TxFreeOutputs(tABC_TxOutput **aOutputs, unsigned int count)
{
    if ((aOutputs != NULL) && (count > 0))
    {
        for (int i = 0; i < count; i++)
        {
            ABC_TxFreeOutput(aOutputs[i]);
        }
        ABC_CLEAR_FREE(aOutputs, sizeof(tABC_TxOutput *) * count);
    }
}

/**
 * Gets the addresses associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param paAddresses       Pointer to store array of addresses info pointers
 * @param pCount            Pointer to store number of addresses
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxGetAddresses(const char *szUserName,
                           const char *szPassword,
                           const char *szWalletUUID,
                           tABC_TxAddress ***paAddresses,
                           unsigned int *pCount,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szAddrDir = NULL;
    tABC_FileIOList *pFileList = NULL;
    char *szFilename = NULL;
    tABC_TxAddress **aAddresses = NULL;
    unsigned int count = 0;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError)); // we want this as an atomic files system function
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(paAddresses);
    *paAddresses = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // validate the creditials
    ABC_CHECK_RET(ABC_WalletCheckCredentials(szUserName, szPassword, szWalletUUID, pError));

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // if there is a address directory
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szAddrDir, &bExists, pError));

    if (bExists == true)
    {
        ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH + 1);

        // get all the files in the address directory
        ABC_FileIOCreateFileList(&pFileList, szAddrDir, NULL);
        for (int i = 0; i < pFileList->nCount; i++)
        {
            // if this file is a normal file
            if (pFileList->apFiles[i]->type == ABC_FileIOFileType_Regular)
            {
                // create the filename for this address
                sprintf(szFilename, "%s/%s", szAddrDir, pFileList->apFiles[i]->szName);

                // add this address to the array
                ABC_CHECK_RET(ABC_TxLoadAddressAndAppendToArray(szUserName, szPassword, szWalletUUID, szFilename, &aAddresses, &count, pError));
            }
        }
    }

    // if we have more than one, then let's sort them
    if (count > 1)
    {
        // sort the transactions by creation date using qsort
        qsort(aAddresses, count, sizeof(tABC_TxAddress *), ABC_TxAddrPtrCompare);
    }

    // store final results
    *paAddresses = aAddresses;
    aAddresses = NULL;
    *pCount = count;
    count = 0;

exit:
    ABC_FREE_STR(szAddrDir);
    ABC_FREE_STR(szFilename);
    ABC_FileIOFreeFileList(pFileList);
    ABC_TxFreeAddresses(aAddresses, count);

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

/**
 * This function is used to support sorting an array of tTxAddress pointers via qsort.
 * qsort has the following documentation for the required function:
 *
 * Pointer to a function that compares two elements.
 * This function is called repeatedly by qsort to compare two elements. It shall follow the following prototype:
 *
 * int compar (const void* p1, const void* p2);
 *
 * Taking two pointers as arguments (both converted to const void*). The function defines the order of the elements by returning (in a stable and transitive manner):
 * return value	meaning
 * <0	The element pointed by p1 goes before the element pointed by p2
 * 0	The element pointed by p1 is equivalent to the element pointed by p2
 * >0	The element pointed by p1 goes after the element pointed by p2
 *
 */
static
int ABC_TxAddrPtrCompare(const void * a, const void * b)
{
    tABC_TxAddress **ppInfoA = (tABC_TxAddress **)a;
    tABC_TxAddress *pInfoA = (tABC_TxAddress *)*ppInfoA;
    tABC_TxAddress **ppInfoB = (tABC_TxAddress **)b;
    tABC_TxAddress *pInfoB = (tABC_TxAddress *)*ppInfoB;

    if (pInfoA->seq < pInfoB->seq) return -1;
    if (pInfoA->seq == pInfoB->seq) return 0;
    if (pInfoA->seq > pInfoB->seq) return 1;

    return 0;
}

/**
 * Loads the given address and adds it to the end of the array
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szFilename        Filename of address
 * @param paAddress         Pointer to array into which the address will be added
 * @param pCount            Pointer to store number of address (will be updated)
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadAddressAndAppendToArray(const char *szUserName,
                                          const char *szPassword,
                                          const char *szWalletUUID,
                                          const char *szFilename,
                                          tABC_TxAddress ***paAddresses,
                                          unsigned int *pCount,
                                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxAddress *pAddress = NULL;
    tABC_TxAddress **aAddresses = NULL;
    unsigned int count = 0;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No transaction filename provided");
    ABC_CHECK_NULL(paAddresses);
    ABC_CHECK_NULL(pCount);

    // hold on to current values
    count = *pCount;
    aAddresses = *paAddresses;

    // load the address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(szUserName, szPassword, szWalletUUID, szFilename, &pAddress, pError));

    // create space for new entry
    if (aAddresses == NULL)
    {
        ABC_ALLOC(aAddresses, sizeof(tABC_TxAddress *));
        count = 1;
    }
    else
    {
        count++;
        ABC_REALLOC(aAddresses, sizeof(tABC_TxAddress *) * count);
    }

    // add it to the array
    aAddresses[count - 1] = pAddress;
    pAddress = NULL;

    // assign the values to the caller
    *paAddresses = aAddresses;
    *pCount = count;

exit:
    ABC_TxFreeAddress(pAddress);

    return cc;
}

#if 0
/**
 * For debug purposes, this prints all of the addresses in the given array
 */
static
void ABC_TxPrintAddresses(tABC_TxAddress **aAddresses, unsigned int count)
{
    if ((aAddresses != NULL) && (count > 0))
    {
        for (int i = 0; i < count; i++)
        {
            ABC_DebugLog("Address - seq: %lld, id: %s, pubAddress: %s\n", aAddresses[i]->seq, aAddresses[i]->szID, aAddresses[i]->szPubAddress);
            if (aAddresses[i]->pDetails)
            {
                ABC_DebugLog("\tDetails - satoshi: %lld, airbitz_fee: %lld, miners_fee: %lld, currency: %lf, name: %s, bizid: %u, category: %s, notes: %s, attributes: %u\n",
                             aAddresses[i]->pDetails->amountSatoshi,
                             aAddresses[i]->pDetails->amountFeesAirbitzSatoshi,
                             aAddresses[i]->pDetails->amountFeesMinersSatoshi,
                             aAddresses[i]->pDetails->amountCurrency,
                             aAddresses[i]->pDetails->szName,
                             aAddresses[i]->pDetails->bizId,
                             aAddresses[i]->pDetails->szCategory,
                             aAddresses[i]->pDetails->szNotes,
                             aAddresses[i]->pDetails->attributes);
            }
            else
            {
                ABC_DebugLog("\tNo Details");
            }
            if (aAddresses[i]->pStateInfo)
            {
                ABC_DebugLog("\tState Info - timeCreation: %lld, recycleable: %s\n",
                             aAddresses[i]->pStateInfo->timeCreation,
                             aAddresses[i]->pStateInfo->bRecycleable ? "yes" : "no");
                if (aAddresses[i]->pStateInfo->aActivities && aAddresses[i]->pStateInfo->countActivities)
                {
                    for (int nActivity = 0; nActivity < aAddresses[i]->pStateInfo->countActivities; nActivity++)
                    {
                        ABC_DebugLog("\t\tActivity - txID: %s, timeCreation: %lld, satoshi: %lld\n",
                                     aAddresses[i]->pStateInfo->aActivities[nActivity].szTxID,
                                     aAddresses[i]->pStateInfo->aActivities[nActivity].timeCreation,
                                     aAddresses[i]->pStateInfo->aActivities[nActivity].amountSatoshi);
                    }
                }
                else
                {
                    ABC_DebugLog("\t\tNo Activities");
                }
            }
            else
            {
                ABC_DebugLog("\tNo State Info");
            }
        }
        typedef struct sTxAddressActivity
        {
            char    *szTxID; // ntxid from bitcoin associated with this activity
            int64_t timeCreation;
            int64_t amountSatoshi;
        } tTxAddressActivity;

        typedef struct sTxAddressStateInfo
        {
            int64_t             timeCreation;
            bool                bRecycleable;
            unsigned int        countActivities;
            tTxAddressActivity  *aActivities;
        } tTxAddressStateInfo;

        typedef struct sABC_TxAddress
        {
            int64_t             seq; // sequence number
            char                *szID; // sequence number in string form
            char                *szPubAddress; // public address
            tABC_TxDetails      *pDetails;
            tTxAddressStateInfo *pStateInfo;
        } tABC_TxAddress;
    }
    else
    {
        ABC_DebugLog("No addresses");
    }
}
#endif

/**
 * Locks the mutex
 *
 * ABC_Tx uses the same mutex as ABC_Login/ABC_Wallet so that there will be no situation in
 * which one thread is in ABC_Tx locked on a mutex and calling a thread safe ABC_Login/ABC_Wallet call
 * that is locked from another thread calling a thread safe ABC_Tx call.
 * In other words, since they call each other, they need to share a recursive mutex.
 */
static
tABC_CC ABC_TxMutexLock(tABC_Error *pError)
{
    return ABC_MutexLock(pError);
}

/**
 * Unlocks the mutex
 */
static
tABC_CC ABC_TxMutexUnlock(tABC_Error *pError)
{
    return ABC_MutexUnlock(pError);
}

/**
 * Adds a transaction to an address's activity log.
 *
 * @param pAddress the address to modify
 * @param pTx the transaction to add to the address
 */
static
tABC_CC ABC_TxAddressAddTx(tABC_TxAddress *pAddress, tABC_Tx *pTx, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    unsigned int countActivities;
    tTxAddressActivity *aActivities = NULL;

    // grow the array:
    countActivities = pAddress->pStateInfo->countActivities;
    aActivities = pAddress->pStateInfo->aActivities;
    ABC_REALLOC(aActivities, sizeof(tTxAddressActivity)*(countActivities + 1));

    // fill in the new entry:
    ABC_STRDUP(aActivities[countActivities].szTxID, pTx->szID);
    aActivities[countActivities].timeCreation = pTx->pStateInfo->timeCreation;
    aActivities[countActivities].amountSatoshi = pTx->pDetails->amountSatoshi;

    // save the array:
    pAddress->pStateInfo->countActivities = countActivities + 1;
    pAddress->pStateInfo->aActivities = aActivities;
    aActivities = NULL;

exit:
    ABC_FREE(aActivities);

    return cc;
}

tABC_CC ABC_TxTransactionExists(const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                const char *szID,
                                tABC_Tx **pTx,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;
    bool bExists = false;

    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szID);
    ABC_CHECK_ASSERT(strlen(szID) > 0, ABC_CC_Error, "No transaction ID provided");

    // validate the creditials
    ABC_CHECK_RET(ABC_WalletCheckCredentials(szUserName, szPassword, szWalletUUID, pError));

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, szID, true, pError));
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));

    // if the internal doesn't exist
    if (bExists == false)
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, szID, false, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    }
    if (bExists)
    {
        ABC_CHECK_RET(ABC_TxLoadTransaction(szUserName, szPassword, szWalletUUID, szFilename, pTx, pError));
    } else {
        *pTx = NULL;
    }
exit:
    ABC_FREE_STR(szFilename);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * This implemens the KMP failure function. Its the preprocessing before we can
 * search for substrings.
 *
 * @param needle - The string to preprocess
 * @param table - An array of integers the string length of needle
 *
 * Returns 1 if a match is found otherwise returns 0
 *
 */
static
void ABC_TxStrTable(const char *needle, int *table)
{
    int pos = 2, cnd = 0;
    table[0] = -1;
    table[1] = 0;
    size_t needle_size = strlen(needle);

    while (pos < needle_size)
    {
        if (tolower(needle[pos - 1]) == tolower(needle[cnd]))
        {
            cnd = cnd + 1;
            table[pos] = cnd;
            pos = pos + 1;
        }
        else if (cnd > 0)
            cnd = table[cnd];
        else
        {
            table[pos] = 0;
            pos = pos + 1;
        }
    }
}

/**
 * This function implemens the KMP string searching algo. This function is
 * used for string matching when searching transactions.
 *
 * @param haystack - The string to search
 * @param needle - The string to find in the haystack
 *
 * Returns 1 if a match is found otherwise returns 0
 *
 */
static
int ABC_TxStrStr(const char *haystack, const char *needle,
                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    if (haystack == NULL || needle == NULL) {
        return 0;
    }
    int m = 0, i = 0, result = -1;
    size_t haystack_size;
    size_t needle_size;
    int *table;

    haystack_size = strlen(haystack);
    needle_size = strlen(needle);

    if (haystack_size == 0 || needle_size == 0) {
        return 0;
    }

    ABC_ALLOC(table, needle_size * sizeof(int));
    ABC_TxStrTable(needle, table);

    while (m + i < haystack_size)
    {
        if (tolower(needle[i]) == tolower(haystack[m + i]))
        {
            if (i == needle_size - 1)
            {
                result = m;
                break;
            }
            i = i + 1;
        }
        else
        {
            if (table[i] > -1)
            {
                i = table[i];
                m = m + i - table[i];
            }
            else
            {
                i = 0;
                m = m + 1;
            }
        }
    }
exit:
    ABC_FREE(table);
    return result > -1 ? 1 : 0;
}

static int
ABC_TxCopyOuputs(tABC_Tx *pTx, tABC_TxOutput **aOutputs, int countOutputs, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int i;

    ABC_CHECK_NULL(pTx);
    ABC_CHECK_NULL(aOutputs);

    pTx->countOutputs = countOutputs;
    if (pTx->countOutputs > 0)
    {
        ABC_ALLOC(pTx->aOutputs, sizeof(tABC_TxOutput *) * pTx->countOutputs);
        for (i = 0; i < countOutputs; ++i)
        {
            ABC_DebugLog("Saving Outputs: %s\n", aOutputs[i]->szAddress);
            ABC_ALLOC(pTx->aOutputs[i], sizeof(tABC_TxOutput));
            ABC_STRDUP(pTx->aOutputs[i]->szAddress, aOutputs[i]->szAddress);
            ABC_STRDUP(pTx->aOutputs[i]->szTxId, aOutputs[i]->szTxId);
            pTx->aOutputs[i]->input = aOutputs[i]->input;
            pTx->aOutputs[i]->value = aOutputs[i]->value;
        }
    }
exit:
    return cc;
}

static int
ABC_TxTransferPopulate(tABC_TxSendInfo *pInfo,
                       tABC_Tx *pTx, tABC_Tx *pReceiveTx,
                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    // Populate Send Tx
    if (pInfo->szSrcName)
    {
        ABC_FREE_STR(pTx->pDetails->szName);
        ABC_STRDUP(pTx->pDetails->szName, pInfo->szSrcName);
    }
    if (pInfo->szSrcCategory)
    {
        ABC_FREE_STR(pTx->pDetails->szCategory);
        ABC_STRDUP(pTx->pDetails->szCategory, pInfo->szSrcCategory);
    }

    // Populate Recv Tx
    if (pInfo->szDestName)
    {
        ABC_FREE_STR(pReceiveTx->pDetails->szName);
        ABC_STRDUP(pReceiveTx->pDetails->szName, pInfo->szDestName);
    }

    if (pInfo->szDestCategory)
    {
        ABC_FREE_STR(pReceiveTx->pDetails->szCategory);
        ABC_STRDUP(pReceiveTx->pDetails->szCategory, pInfo->szDestCategory);
    }
exit:
    return cc;
}

#if NETWORK_FAKE
// /////////////////////////////////////////////////////////////////////////////
// Fake code begins here.
// /////////////////////////////////////////////////////////////////////////////

typedef struct sABC_FakeRecieveInfo
{
    char *szUserName;
    char *szPassword;
    char *szWalletUUID;
    char *szAddress;
} tABC_FakeRecieveInfo;

static void ABC_TxFreeFakeRecieveInfo(tABC_FakeRecieveInfo *pInfo)
{
    if (pInfo)
    {
        ABC_FREE_STR(pInfo->szUserName);
        ABC_FREE_STR(pInfo->szPassword);
        ABC_FREE_STR(pInfo->szWalletUUID);
        ABC_FREE_STR(pInfo->szAddress);
        ABC_FREE(pInfo);
    }
}

/**
 * Launches a thread with a few-second delay. Once the thread wakes up, it
 * creates a fake recieve transaction.
 */
static
tABC_CC ABC_TxKickoffFakeReceive(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 const char *szAddress,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_FakeRecieveInfo *pInfo = NULL;
    pthread_t thread;
    int rv = 0;

    // copy the parameters:
    ABC_ALLOC(pInfo, sizeof(tABC_FakeRecieveInfo));
    ABC_STRDUP(pInfo->szUserName,   szUserName);
    ABC_STRDUP(pInfo->szPassword,   szPassword);
    ABC_STRDUP(pInfo->szWalletUUID, szWalletUUID);
    ABC_STRDUP(pInfo->szAddress,    szAddress);

    // launch the thread:
    rv = pthread_create(&thread, NULL, ABC_TxFakeReceiveThread, pInfo);
    ABC_CHECK_ASSERT(0 == rv, ABC_CC_SysError, "Cannot start fake thread.");

    pInfo = NULL;
exit:
    ABC_TxFreeFakeRecieveInfo(pInfo);

    return cc;
}

/**
 * The thread for creating fake recieve transactions.
 */
static void *ABC_TxFakeReceiveThread(void *pData)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Error *pError = NULL;
    tABC_FakeRecieveInfo *pInfo = pData;

    tABC_TxAddress *pAddress = NULL;
    tABC_U08Buf TxID = ABC_BUF_NULL;
    tABC_U08Buf TxMalID = ABC_BUF_NULL;
    tABC_Tx *pTx = NULL;
    tABC_U08Buf IncomingAddress = ABC_BUF_NULL;

    // delay for simulation
    sleep(2);

    // grab the address
    ABC_CHECK_RET(ABC_TxLoadAddress(pInfo->szUserName, pInfo->szPassword,
                                    pInfo->szWalletUUID, pInfo->szAddress,
                                    &pAddress, pError));

    // create a transaction
    ABC_ALLOC(pTx, sizeof(tABC_Tx));
    ABC_ALLOC(pTx->pStateInfo, sizeof(tTxStateInfo));

    // copy the details
    ABC_CHECK_RET(ABC_TxDupDetails(&(pTx->pDetails), pAddress->pDetails, pError));

    // set the state
    pTx->pStateInfo->timeCreation = time(NULL);
    pTx->pStateInfo->bInternal = false;

    // create a random transaction id
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(32, &TxID, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(TxID, &(pTx->szID), pError));

    // create a random malleable transaction id
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(32, &TxMalID, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(TxMalID, &(pTx->pStateInfo->szMalleableTxId), pError));

    // save the transaction
    ABC_CHECK_RET(ABC_TxSaveTransaction(pInfo->szUserName, pInfo->szPassword, pInfo->szWalletUUID, pTx, pError));

    // add the transaction to the address
    ABC_CHECK_RET(ABC_TxAddressAddTx(pAddress, pTx, pError));

    // set the address as not recycled so it doens't get used again
    pAddress->pStateInfo->bRecycleable = false;

    // save the address
    ABC_CHECK_RET(ABC_TxSaveAddress(pInfo->szUserName, pInfo->szPassword, pInfo->szWalletUUID, pAddress, pError));

    // alert the GUI
    if (gfAsyncBitCoinEventCallback)
    {
        tABC_AsyncBitCoinInfo info;
        info.pData = pAsyncBitCoinCallerData;
        info.eventType = ABC_AsyncEventType_IncomingBitCoin;
        ABC_STRDUP(info.szTxID, pTx->szID);
        ABC_STRDUP(info.szWalletUUID, pInfo->szWalletUUID);
        ABC_STRDUP(info.szDescription, "Received fake funds");
        gfAsyncBitCoinEventCallback(&info);
        ABC_FREE_STR(info.szTxID);
        ABC_FREE_STR(info.szWalletUUID);
        ABC_FREE_STR(info.szDescription);
    }

    //printf("We are here %s %s %s", pInfo->szUserName, pInfo->szWalletUUID, pInfo->szAddress);

exit:
    ABC_TxFreeFakeRecieveInfo(pInfo);
    ABC_TxFreeAddress(pAddress);
    ABC_BUF_FREE(TxID);
    ABC_BUF_FREE(IncomingAddress);
    ABC_TxFreeTx(pTx);

    return NULL;
}

static
tABC_CC ABC_TxFakeSend(tABC_TxSendInfo  *pInfo, char **pszTxID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_Tx *pTx = NULL;
    tABC_U08Buf TxID = ABC_BUF_NULL;
    tABC_U08Buf TxMalID = ABC_BUF_NULL;
    tABC_Tx *pReceiveTx = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(pInfo);
    ABC_CHECK_NULL(pszTxID);

    // take this non-blocking opportunity to update the info from the server if needed
    ABC_CHECK_RET(ABC_GeneralUpdateInfo(pError));

    // create a transaction
    ABC_ALLOC(pTx, sizeof(tABC_Tx));
    ABC_ALLOC(pTx->pStateInfo, sizeof(tTxStateInfo));

    // copy the details
    ABC_CHECK_RET(ABC_TxDupDetails(&(pTx->pDetails), pInfo->pDetails, pError));
    // Make sure values are negative
    if (pTx->pDetails->amountSatoshi > 0)
    {
        pTx->pDetails->amountSatoshi *= -1;
    }
    if (pTx->pDetails->amountCurrency > 0)
    {
        pTx->pDetails->amountCurrency *= -1.0;
    }

    // set the state
    pTx->pStateInfo->timeCreation = time(NULL);
    pTx->pStateInfo->bInternal = true;

    // create a random transaction id
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(32, &TxID, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(TxID, &(pTx->szID), pError));

    ABC_CHECK_RET(ABC_CryptoCreateRandomData(32, &TxMalID, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(TxMalID, &(pTx->pStateInfo->szMalleableTxId), pError));

    if (pInfo->bTransfer)
    {
        ABC_ALLOC(pReceiveTx, sizeof(tABC_Tx));
        ABC_ALLOC(pReceiveTx->pStateInfo, sizeof(tTxStateInfo));

        ABC_CHECK_RET(ABC_TxDupDetails(&(pReceiveTx->pDetails), pInfo->pDetails, pError));
        // Make sure values are positive
        if (pReceiveTx->pDetails->amountSatoshi < 0)
        {
            pReceiveTx->pDetails->amountSatoshi *= -1;
        }
        if (pReceiveTx->pDetails->amountCurrency < 0)
        {
            pReceiveTx->pDetails->amountCurrency *= -1.0;
        }
        // set the state
        pReceiveTx->pStateInfo->timeCreation = time(NULL);
        pReceiveTx->pStateInfo->bInternal = true;
        ABC_CHECK_RET(ABC_CryptoHexEncode(TxID, &(pReceiveTx->szID), pError));
        ABC_CHECK_RET(ABC_CryptoHexEncode(TxMalID, &(pReceiveTx->pStateInfo->szMalleableTxId), pError));

        // Set the payee and category for both txs
        ABC_CHECK_RET(ABC_TxTransferPopulate(pInfo, pTx, pReceiveTx, pError));

        // save the transaction
        ABC_CHECK_RET(ABC_TxSaveTransaction(pInfo->szUserName, pInfo->szPassword, pInfo->szDestWalletUUID, pReceiveTx, pError));
    }
    // save the transaction
    ABC_CHECK_RET(ABC_TxSaveTransaction(pInfo->szUserName, pInfo->szPassword, pInfo->szWalletUUID, pTx, pError));

    // Sync the data
    ABC_CHECK_RET(ABC_DataSyncAll(pInfo->szUserName, pInfo->szPassword, pError));

    // set the transaction id for the caller
    ABC_STRDUP(*pszTxID, pTx->szID);
exit:
    ABC_BUF_FREE(TxID);
    ABC_BUF_FREE(TxMalID);
    ABC_TxFreeTx(pTx);
    ABC_TxFreeTx(pReceiveTx);

    ABC_TxMutexUnlock(NULL);
    return cc;
}
#endif // NETWORK_FAKE
