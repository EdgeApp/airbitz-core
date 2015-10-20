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

#include "Tx.hpp"
#include "Context.hpp"
#include "account/Account.hpp"
#include "account/AccountSettings.hpp"
#include "bitcoin/Text.hpp"
#include "bitcoin/Watcher.hpp"
#include "bitcoin/WatcherBridge.hpp"
#include "crypto/Crypto.hpp"
#include "spend/Spend.hpp"
#include "util/Debug.hpp"
#include "util/FileIO.hpp"
#include "util/Mutex.hpp"
#include "util/Util.hpp"
#include "wallet/Address.hpp"
#include "wallet/Details.hpp"
#include "wallet/TxMetadata.hpp"
#include "wallet/Wallet.hpp"
#include <bitcoin/bitcoin.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <string>

namespace abcd {

#define TX_INTERNAL_SUFFIX                      "-int.json" // the transaction was created by our direct action (i.e., send)
#define TX_EXTERNAL_SUFFIX                      "-ext.json" // the transaction was created due to events in the block-chain (usually receives)

#define JSON_CREATION_DATE_FIELD                "creationDate"
#define JSON_MALLEABLE_TX_ID                    "malleableTxId"

#define JSON_TX_NTXID_FIELD                     "ntxid"
#define JSON_TX_STATE_FIELD                     "state"
#define JSON_TX_INTERNAL_FIELD                  "internal"

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
    char    *szTxid;
} tTxStateInfo;

typedef struct sABC_Tx
{
    char            *szNtxid;
    tABC_TxDetails  *pDetails;
    tTxStateInfo    *pStateInfo;
} tABC_Tx;

static tABC_CC  ABC_TxCheckForInternalEquivalent(const char *szFilename, bool *pbEquivalent, tABC_Error *pError);
static tABC_CC  ABC_TxGetTxTypeAndBasename(const char *szFilename, tTxType *pType, char **pszBasename, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTransactionInfo(Wallet &self, const char *szFilename, tABC_TxInfo **ppTransaction, tABC_Error *pError);
static Status   txGetOutputs(Wallet &self, const std::string &ntxid, tABC_TxOutput ***paOutputs, unsigned int *pCount);
static tABC_CC  ABC_TxLoadTxAndAppendToArray(Wallet &self, int64_t startTime, int64_t endTime, const char *szFilename, tABC_TxInfo ***paTransactions, unsigned int *pCount, tABC_Error *pError);
static tABC_CC  ABC_TxCreateTxFilename(Wallet &self, char **pszFilename, const std::string &ntxid, bool bInternal, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTransaction(Wallet &self, const char *szFilename, tABC_Tx **ppTx, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeTxState(json_t *pJSON_Obj, tTxStateInfo **ppInfo, tABC_Error *pError);
static void     ABC_TxFreeTx(tABC_Tx *pTx);
static tABC_CC  ABC_TxSaveTransaction(Wallet &self, const tABC_Tx *pTx, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeTxState(json_t *pJSON_Obj, tTxStateInfo *pInfo, tABC_Error *pError);
static int      ABC_TxInfoPtrCompare (const void * a, const void * b);
static tABC_CC  ABC_TxTransactionExists(Wallet &self, const std::string &ntxid, tABC_Tx **pTx, tABC_Error *pError);
static void     ABC_TxStrTable(const char *needle, int *table);
static int      ABC_TxStrStr(const char *haystack, const char *needle, tABC_Error *pError);
static tABC_CC  ABC_TxSaveNewTx(Wallet &self, tABC_Tx *pTx, const std::vector<std::string> &addresses, bool bOutside, tABC_Error *pError);
static tABC_CC  ABC_TxCalcCurrency(Wallet &self, int64_t amountSatoshi, double *pCurrency, tABC_Error *pError);

tABC_CC ABC_TxSendComplete(Wallet &self,
                           SendInfo         *pInfo,
                           const std::string &ntxid,
                           const std::string &txid,
                           const std::vector<std::string> &addresses,
                           tABC_Error       *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    tABC_Tx *pTx = structAlloc<tABC_Tx>();
    tABC_Tx *pReceiveTx = NULL;
    double currency;
    Address address;

    // Start watching all addresses incuding new change addres
    ABC_CHECK_RET(ABC_TxWatchAddresses(self, pError));

    // set the state
    pTx->pStateInfo = structAlloc<tTxStateInfo>();
    pTx->pStateInfo->timeCreation = time(NULL);
    pTx->pStateInfo->bInternal = true;
    pTx->pStateInfo->szTxid = stringCopy(txid);

    // copy the details
    ABC_CHECK_RET(ABC_TxDetailsCopy(&(pTx->pDetails), pInfo->pDetails, pError));

    // Add in tx fees to the amount of the tx
    if (pInfo->szDestAddress && self.addresses.get(address, pInfo->szDestAddress))
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

    ABC_CHECK_RET(ABC_TxCalcCurrency(
        self, pTx->pDetails->amountSatoshi, &currency, pError));
    pTx->pDetails->amountCurrency = currency;

    if (pTx->pDetails->amountSatoshi > 0)
        pTx->pDetails->amountSatoshi *= -1;
    if (pTx->pDetails->amountCurrency > 0)
        pTx->pDetails->amountCurrency *= -1.0;

    // Store transaction ID
    pTx->szNtxid = stringCopy(ntxid);

    // Save the transaction:
    ABC_CHECK_RET(ABC_TxSaveNewTx(self, pTx, addresses, false, pError));

    if (pInfo->bTransfer)
    {
        pReceiveTx = structAlloc<tABC_Tx>();
        pReceiveTx->pStateInfo = structAlloc<tTxStateInfo>();

        // set the state
        pReceiveTx->pStateInfo->timeCreation = time(NULL);
        pReceiveTx->pStateInfo->bInternal = true;
        pReceiveTx->pStateInfo->szTxid = stringCopy(txid);

        // copy the details
        ABC_CHECK_RET(ABC_TxDetailsCopy(&(pReceiveTx->pDetails), pInfo->pDetails, pError));

        // Set the payee name:
        ABC_FREE_STR(pReceiveTx->pDetails->szName);
        pReceiveTx->pDetails->szName = stringCopy(self.name());

        pReceiveTx->pDetails->amountSatoshi = pInfo->pDetails->amountSatoshi;

        //
        // Since this wallet is receiving, it didn't really get charged AB fees
        // This should really be an assert since no transfers should have AB fees
        //
        pReceiveTx->pDetails->amountFeesAirbitzSatoshi = 0;

        ABC_CHECK_RET(ABC_TxCalcCurrency(*pInfo->walletDest,
            pReceiveTx->pDetails->amountSatoshi, &pReceiveTx->pDetails->amountCurrency, pError));

        if (pReceiveTx->pDetails->amountSatoshi < 0)
            pReceiveTx->pDetails->amountSatoshi *= -1;
        if (pReceiveTx->pDetails->amountCurrency < 0)
            pReceiveTx->pDetails->amountCurrency *= -1.0;

        // Store transaction ID
        pReceiveTx->szNtxid = stringCopy(ntxid);

        // save the transaction
        ABC_CHECK_RET(ABC_TxSaveNewTx(*pInfo->walletDest, pReceiveTx, addresses, false, pError));
    }

exit:
    ABC_TxFreeTx(pTx);
    ABC_TxFreeTx(pReceiveTx);
    return cc;
}

/**
 * Handles creating or updating when we receive a transaction
 */
tABC_CC ABC_TxReceiveTransaction(Wallet &self,
                                 uint64_t amountSatoshi, uint64_t feeSatoshi,
                                 const std::string &ntxid,
                                 const std::string &txid,
                                 const std::vector<std::string> &addresses,
                                 tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                                 void *pData,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    tABC_Tx *pTx = NULL;
    double currency = 0.0;

    // Does the transaction already exist?
    ABC_TxTransactionExists(self, ntxid, &pTx, pError);
    if (pTx == NULL)
    {
        ABC_CHECK_RET(ABC_TxCalcCurrency(self, amountSatoshi, &currency, pError));

        // create a transaction
        pTx = structAlloc<tABC_Tx>();
        pTx->pStateInfo = structAlloc<tTxStateInfo>();
        pTx->pDetails = structAlloc<tABC_TxDetails>();

        pTx->pStateInfo->szTxid = stringCopy(txid);
        pTx->pStateInfo->timeCreation = time(NULL);
        pTx->pDetails->amountSatoshi = amountSatoshi;
        pTx->pDetails->amountCurrency = currency;
        pTx->pDetails->amountFeesMinersSatoshi = feeSatoshi;

        pTx->pDetails->szName = stringCopy("");
        pTx->pDetails->szCategory = stringCopy("");
        pTx->pDetails->szNotes = stringCopy("");

        // set the state
        pTx->pStateInfo->timeCreation = time(NULL);
        pTx->pStateInfo->bInternal = false;

        // store transaction id
        pTx->szNtxid = stringCopy(ntxid);

        // add the transaction to the address
        ABC_CHECK_RET(ABC_TxSaveNewTx(self, pTx, addresses, true, pError));

        // Mark the wallet cache as dirty in case the Tx wasn't included in the current balance
        self.balanceDirty();

        if (fAsyncBitCoinEventCallback)
        {
            tABC_AsyncBitCoinInfo info;
            info.pData = pData;
            info.eventType = ABC_AsyncEventType_IncomingBitCoin;
            info.szTxID = stringCopy(pTx->szNtxid);
            info.szWalletUUID = stringCopy(self.id());
            info.szDescription = stringCopy("Received funds");
            fAsyncBitCoinEventCallback(&info);
            ABC_FREE_STR(info.szTxID);
            ABC_FREE_STR(info.szDescription);
        }
    }
    else
    {
        ABC_DebugLog("We already have %s", ntxid.c_str());

        // Mark the wallet cache as dirty in case the Tx wasn't included in the current balance
        self.balanceDirty();

        if (fAsyncBitCoinEventCallback)
        {
            tABC_AsyncBitCoinInfo info;
            info.pData = pData;
            info.eventType = ABC_AsyncEventType_DataSyncUpdate;
            info.szTxID = stringCopy(pTx->szNtxid);
            info.szWalletUUID = stringCopy(self.id());
            info.szDescription = stringCopy("Updated balance");
            fAsyncBitCoinEventCallback(&info);
            ABC_FREE_STR(info.szTxID);
            ABC_FREE_STR(info.szDescription);
        }
    }
exit:
    ABC_TxFreeTx(pTx);

    return cc;
}

/**
 * Saves the a never-before-seen transaction to the sync database,
 * updating the metadata as appropriate.
 *
 * @param bOutside true if this is an outside transaction that needs its
 * details populated from the address database.
 */
tABC_CC
ABC_TxSaveNewTx(Wallet &self,
                tABC_Tx *pTx,
                const std::vector<std::string> &addresses,
                bool bOutside,
                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    // Mark addresses as used:
    TxMetadata metadata;
    for (const auto &i: addresses)
    {
        Address address;
        if (self.addresses.get(address, i))
        {
            // Update the transaction:
            if (address.recyclable)
            {
                address.recyclable = false;
                ABC_CHECK_NEW(self.addresses.save(address));
            }
            metadata = address.metadata;
        }
    }

    // Copy the metadata (if any):
    if (bOutside)
    {
        if (!ABC_STRLEN(pTx->pDetails->szName) && !metadata.name.empty())
            pTx->pDetails->szName = stringCopy(metadata.name);
        if (!ABC_STRLEN(pTx->pDetails->szNotes) && !metadata.notes.empty())
            pTx->pDetails->szNotes = stringCopy(metadata.notes);
        if (!ABC_STRLEN(pTx->pDetails->szCategory) && !metadata.category.empty())
            pTx->pDetails->szCategory = stringCopy(metadata.category);
    }
    ABC_CHECK_RET(ABC_TxSaveTransaction(self, pTx, pError));

exit:
    return cc;
}

/**
 * Calculates the amount of currency based off of Wallet's currency code
 */
static
tABC_CC ABC_TxCalcCurrency(Wallet &self, int64_t amountSatoshi,
                           double *pCurrency, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    double currency = 0.0;

    ABC_CHECK_NEW(gContext->exchangeCache.satoshiToCurrency(
        currency, amountSatoshi, static_cast<Currency>(self.currency())));

    *pCurrency = currency;

exit:
    return cc;
}

/**
 * Get the specified transactions.
 * @param ppTransaction     Location to store allocated transaction
 *                          (caller must free)
 */
tABC_CC ABC_TxGetTransaction(Wallet &self,
                             const std::string &ntxid,
                             tABC_TxInfo **ppTransaction,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;
    tABC_TxInfo *pTransaction = NULL;

    *ppTransaction = NULL;

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, ntxid, true, pError));
    if (!fileExists(szFilename))
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, ntxid, false, pError));
    }

    ABC_CHECK_ASSERT(fileExists(szFilename), ABC_CC_NoTransaction, "Transaction does not exist");

    // load the existing transaction
    ABC_CHECK_RET(ABC_TxLoadTransactionInfo(self, szFilename, &pTransaction, pError));

    // assign final result
    *ppTransaction = pTransaction;
    pTransaction = NULL;

exit:
    ABC_FREE_STR(szFilename);
    ABC_TxFreeTx(pTx);
    ABC_TxFreeTransaction(pTransaction);

    return cc;
}

/**
 * Gets the transactions associated with the given wallet.
 *
 * @param startTime         Return transactions after this time
 * @param endTime           Return transactions before this time
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetTransactions(Wallet &self,
                              int64_t startTime,
                              int64_t endTime,
                              tABC_TxInfo ***paTransactions,
                              unsigned int *pCount,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    AutoFileLock fileLock(gFileMutex); // We are iterating over the filesystem

    std::string txDir = self.txDir();
    tABC_FileIOList *pFileList = NULL;
    tABC_TxInfo **aTransactions = NULL;
    unsigned int count = 0;

    *paTransactions = NULL;
    *pCount = 0;

    // if there is a transaction directory
    if (fileExists(txDir))
    {
        // get all the files in the transaction directory
        ABC_FileIOCreateFileList(&pFileList, txDir.c_str(), NULL);
        for (int i = 0; i < pFileList->nCount; i++)
        {
            // if this file is a normal file
            if (pFileList->apFiles[i]->type == ABC_FileIOFileType_Regular)
            {
                auto path = txDir + pFileList->apFiles[i]->szName;

                // get the transaction type
                tTxType type = TxType_None;
                ABC_CHECK_RET(ABC_TxGetTxTypeAndBasename(path.c_str(), &type, NULL, pError));

                // if this is a transaction file (based upon name)
                if (type != TxType_None)
                {
                    bool bHasInternalEquivalent = false;

                    // if this is an external transaction
                    if (type == TxType_External)
                    {
                        // check if it has an internal equivalent and, if so, delete the external
                        ABC_CHECK_RET(ABC_TxCheckForInternalEquivalent(path.c_str(), &bHasInternalEquivalent, pError));
                    }

                    // if this doesn't not have an internal equivalent (or is an internal itself)
                    if (!bHasInternalEquivalent)
                    {
                        // add this transaction to the array

                        ABC_CHECK_RET(ABC_TxLoadTxAndAppendToArray(self,
                                                                   startTime,
                                                                   endTime,
                                                                   path.c_str(),
                                                                   &aTransactions,
                                                                   &count,
                                                                   pError));
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
    ABC_FileIOFreeFileList(pFileList);
    ABC_TxFreeTransactions(aTransactions, count);

    return cc;
}

/**
 * Searches transactions associated with the given wallet.
 *
 * @param szQuery           Query to search
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxSearchTransactions(Wallet &self,
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

    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    ABC_CHECK_NULL(paTransactions);
    *paTransactions = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    ABC_TxGetTransactions(self, ABC_GET_TX_ALL_TIMES, ABC_GET_TX_ALL_TIMES,
                          &aTransactions, &count, pError);
    ABC_ARRAY_NEW(aSearchTransactions, count, tABC_TxInfo*);
    for (i = 0; i < count; i++)
    {
        tABC_TxInfo *pInfo = aTransactions[i];
        auto satoshi = std::to_string(pInfo->pDetails->amountSatoshi);
        auto currency = std::to_string(pInfo->pDetails->amountCurrency);
        if (ABC_TxStrStr(satoshi.c_str(), szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        }
        if (ABC_TxStrStr(currency.c_str(), szQuery, pError))
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
        ABC_ARRAY_RESIZE(aSearchTransactions, matchCount, tABC_TxInfo*);
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
    tTxType type = TxType_None;

    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(pbEquivalent);
    *pbEquivalent = false;

    // get the type and the basename of this transaction
    ABC_CHECK_RET(ABC_TxGetTxTypeAndBasename(szFilename, &type, &szBasename, pError));

    // if this is an external
    if (type == TxType_External)
    {
        std::string name = std::string(szBasename) + TX_INTERNAL_SUFFIX;

        // if the internal version exists
        if (fileExists(name))
        {
            // delete the external version (this one)
            ABC_CHECK_NEW(fileDelete(szFilename));

            *pbEquivalent = true;
        }
    }

exit:
    ABC_FREE_STR(szBasename);

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
    unsigned sizeSuffix = 0;

    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(pType);

    // assume nothing found
    *pType = TxType_None;
    if (pszBasename != NULL)
    {
        *pszBasename = NULL;
    }

    // look for external the suffix
    sizeSuffix = strlen(TX_EXTERNAL_SUFFIX);
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
                szBasename = stringCopy(szFilename);
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
                    szBasename = stringCopy(szFilename);
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
 * Prepares transaction outputs for the advanced details screen.
 */
static Status
txGetOutputs(Wallet &self, const std::string &ntxid,
    tABC_TxOutput ***paOutputs, unsigned int *pCount)
{
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));

    bc::hash_digest hash;
    if (!bc::decode_hash(hash, ntxid))
        return ABC_ERROR(ABC_CC_ParseError, "Bad txid");
    auto tx = watcher->db().ntxidLookup(hash);
    auto txid = bc::encode_hash(bc::hash_transaction(tx));

    // Create the array:
    size_t count = tx.inputs.size() + tx.outputs.size();
    tABC_TxOutput **aOutputs = (tABC_TxOutput**)calloc(count, sizeof(tABC_TxOutput*));
    if (!aOutputs)
        return ABC_ERROR(ABC_CC_NULLPtr, "out of memory");

    // Build output entries:
    int i = 0;
    for (const auto &input: tx.inputs)
    {
        auto prev = input.previous_output;
        bc::payment_address addr;
        bc::extract(addr, input.script);

        tABC_TxOutput *out = (tABC_TxOutput *) malloc(sizeof(tABC_TxOutput));
        out->input = true;
        out->szTxId = stringCopy(bc::encode_hash(prev.hash));
        out->szAddress = stringCopy(addr.encoded());

        auto tx = watcher->db().txidLookup(prev.hash);
        if (prev.index < tx.outputs.size())
        {
            out->value = tx.outputs[prev.index].value;
        }
        aOutputs[i] = out;
        i++;
    }
    for (const auto &output: tx.outputs)
    {
        bc::payment_address addr;
        bc::extract(addr, output.script);

        tABC_TxOutput *out = (tABC_TxOutput *) malloc(sizeof(tABC_TxOutput));
        out->input = false;
        out->value = output.value;
        out->szTxId = stringCopy(txid);
        out->szAddress = stringCopy(addr.encoded());

        aOutputs[i] = out;
        i++;
    }

    *paOutputs = aOutputs;
    *pCount = count;
    return Status();
}

/**
 * Load the specified transaction info.
 *
 * @param szFilename        Filename of the transaction
 * @param ppTransaction     Location to store allocated transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadTransactionInfo(Wallet &self,
                                  const char *szFilename,
                                  tABC_TxInfo **ppTransaction,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    tABC_Tx *pTx = NULL;
    tABC_TxInfo *pTransaction = structAlloc<tABC_TxInfo>();

    *ppTransaction = NULL;

    // load the transaction
    ABC_CHECK_RET(ABC_TxLoadTransaction(self, szFilename, &pTx, pError));
    ABC_CHECK_NULL(pTx->pDetails);
    ABC_CHECK_NULL(pTx->pStateInfo);

    // steal the data and assign it to our new struct
    pTransaction->szID = stringCopy(pTx->szNtxid);
    pTransaction->szMalleableTxId = stringCopy(pTx->pStateInfo->szTxid);
    pTransaction->timeCreation = pTx->pStateInfo->timeCreation;
    pTransaction->pDetails = pTx->pDetails;
    pTx->pDetails = NULL;
    ABC_CHECK_NEW(txGetOutputs(self, pTx->szNtxid,
        &pTransaction->aOutputs, &pTransaction->countOutputs));

    // assign final result
    *ppTransaction = pTransaction;
    pTransaction = NULL;

exit:
    ABC_TxFreeTx(pTx);
    ABC_TxFreeTransaction(pTransaction);

    return cc;
}

/**
 * Loads the given transaction info and adds it to the end of the array
 *
 * @param szFilename        Filename of transaction
 * @param paTransactions    Pointer to array into which the transaction will be added
 * @param pCount            Pointer to store number of transactions (will be updated)
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadTxAndAppendToArray(Wallet &self,
                                     int64_t startTime,
                                     int64_t endTime,
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

    // hold on to current values
    count = *pCount;
    aTransactions = *paTransactions;

    // load it into the info transaction structure
    ABC_CHECK_RET(ABC_TxLoadTransactionInfo(self, szFilename, &pTransaction, pError));

    if ((endTime == ABC_GET_TX_ALL_TIMES) ||
        (pTransaction->timeCreation >= startTime &&
         pTransaction->timeCreation < endTime))
    {
        // create space for new entry
        if (aTransactions == NULL)
        {
            ABC_ARRAY_NEW(aTransactions, 1, tABC_TxInfo*);
            count = 1;
        }
        else
        {
            count++;
            ABC_ARRAY_RESIZE(aTransactions, count, tABC_TxInfo*);
        }

        // add it to the array
        aTransactions[count - 1] = pTransaction;
        pTransaction = NULL;

        // assign the values to the caller
        *paTransactions = aTransactions;
        *pCount = count;

    }

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
        ABC_TxDetailsFree(pTransaction->pDetails);
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
        for (unsigned i = 0; i < count; i++)
        {
            ABC_TxFreeTransaction(aTransactions[i]);
        }

        ABC_FREE(aTransactions);
    }
}

/**
 * Sets the details for a specific existing transaction.
 * @param pDetails          Details for the transaction
 */
tABC_CC ABC_TxSetTransactionDetails(Wallet &self,
                                    const std::string &ntxid,
                                    tABC_TxDetails *pDetails,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, ntxid, true, pError));
    if (!fileExists(szFilename))
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, ntxid, false, pError));
    }

    ABC_CHECK_ASSERT(fileExists(szFilename), ABC_CC_NoTransaction, "Transaction does not exist");

    // load the existing transaction
    ABC_CHECK_RET(ABC_TxLoadTransaction(self, szFilename, &pTx, pError));
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
    pTx->pDetails->szName = stringCopy(pDetails->szName);
    ABC_FREE_STR(pTx->pDetails->szCategory);
    pTx->pDetails->szCategory = stringCopy(pDetails->szCategory);
    ABC_FREE_STR(pTx->pDetails->szNotes);
    pTx->pDetails->szNotes = stringCopy(pDetails->szNotes);

    // re-save the transaction
    ABC_CHECK_RET(ABC_TxSaveTransaction(self, pTx, pError));

exit:
    ABC_FREE_STR(szFilename);
    ABC_TxFreeTx(pTx);

    return cc;
}

/**
 * Gets the details for a specific existing transaction.
 * @param ppDetails         Location to store allocated details for the transaction
 *                          (caller must free)
 */
tABC_CC ABC_TxGetTransactionDetails(Wallet &self,
                                    const std::string &ntxid,
                                    tABC_TxDetails **ppDetails,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;
    tABC_TxDetails *pDetails = NULL;

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, ntxid, true, pError));
    if (!fileExists(szFilename))
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, ntxid, false, pError));
    }

    ABC_CHECK_ASSERT(fileExists(szFilename), ABC_CC_NoTransaction, "Transaction does not exist");

    // load the existing transaction
    ABC_CHECK_RET(ABC_TxLoadTransaction(self, szFilename, &pTx, pError));
    ABC_CHECK_NULL(pTx->pDetails);
    ABC_CHECK_NULL(pTx->pStateInfo);

    // duplicate the details
    ABC_CHECK_RET(ABC_TxDetailsCopy(&pDetails, pTx->pDetails, pError));

    // assign final result
    *ppDetails = pDetails;
    pDetails = NULL;


exit:
    ABC_FREE_STR(szFilename);
    ABC_TxFreeTx(pTx);
    ABC_TxDetailsFree(pDetails);

    return cc;
}

/**
 * Creates a transaction as a result of a sweep.
 *
 * @param wallet    Wallet ID struct
 * @param txId      Non-Malleable Tx ID
 * @param malTxId   Malleable Tx ID
 * @param funds     Amount of funds swept
 * @param pDetails  Tx Details
 */
tABC_CC ABC_TxSweepSaveTransaction(Wallet &wallet,
                                   const std::string &ntxid,
                                   const std::string &txid,
                                   uint64_t funds,
                                   tABC_TxDetails *pDetails,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Tx *pTx = structAlloc<tABC_Tx>();
    double currency;

    // set the state
    pTx->pStateInfo = structAlloc<tTxStateInfo>();
    pTx->pStateInfo->timeCreation = time(NULL);
    pTx->pStateInfo->bInternal = true;
    pTx->szNtxid = stringCopy(ntxid);
    pTx->pStateInfo->szTxid = stringCopy(txid);

    // Copy the details
    ABC_CHECK_RET(ABC_TxDetailsCopy(&(pTx->pDetails), pDetails, pError));
    pTx->pDetails->amountSatoshi = funds;
    pTx->pDetails->amountFeesAirbitzSatoshi = 0;

    ABC_CHECK_NEW(gContext->exchangeCache.satoshiToCurrency(
        currency, pTx->pDetails->amountSatoshi,
        static_cast<Currency>(wallet.currency())));
    pTx->pDetails->amountCurrency = currency;

    // save the transaction
    ABC_CHECK_RET(ABC_TxSaveTransaction(wallet, pTx, pError));

exit:
    ABC_TxFreeTx(pTx);
    return cc;
}

/**
 * Gets the filename for a given transaction
 * format is: N-Base58(HMAC256(TxID,MK)).json
 *
 * @param pszFilename Output filename name. The caller must free this.
 */
static
tABC_CC ABC_TxCreateTxFilename(Wallet &self, char **pszFilename, const std::string &ntxid, bool bInternal, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string path = self.txDir() + cryptoFilename(self.dataKey(), ntxid) +
        (bInternal ? TX_INTERNAL_SUFFIX : TX_EXTERNAL_SUFFIX);

    *pszFilename = stringCopy(path);

    return cc;
}

/**
 * Loads a transaction from disk
 *
 * @param ppTx  Pointer to location to hold allocated transaction
 *              (it is the callers responsiblity to free this transaction)
 */
static
tABC_CC ABC_TxLoadTransaction(Wallet &self,
                              const char *szFilename,
                              tABC_Tx **ppTx,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    json_t *pJSON_Root = NULL;
    tABC_Tx *pTx = structAlloc<tABC_Tx>();
    json_t *jsonVal = NULL;

    *ppTx = NULL;

    // make sure the transaction exists
    ABC_CHECK_ASSERT(fileExists(szFilename), ABC_CC_NoTransaction, "Transaction does not exist");

    // load the json object (load file, decrypt it, create json object
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, toU08Buf(self.dataKey()), &pJSON_Root, pError));

    // get the id
    jsonVal = json_object_get(pJSON_Root, JSON_TX_NTXID_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing id");
    pTx->szNtxid = stringCopy(json_string_value(jsonVal));

    // get the state object
    ABC_CHECK_RET(ABC_TxDecodeTxState(pJSON_Root, &(pTx->pStateInfo), pError));

    // get the details object
    ABC_CHECK_RET(ABC_TxDetailsDecode(pJSON_Root, &(pTx->pDetails), pError));

    // get advanced details
    ABC_CHECK_RET(
        ABC_BridgeTxDetails(self, pTx->szNtxid,
                            &(pTx->pDetails->amountSatoshi),
                            &(pTx->pDetails->amountFeesMinersSatoshi),
                            pError));
    // assign final result
    *ppTx = pTx;
    pTx = NULL;

exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    ABC_TxFreeTx(pTx);

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

    tTxStateInfo *pInfo = structAlloc<tTxStateInfo>();
    json_t *jsonState = NULL;
    json_t *jsonVal = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppInfo);
    *ppInfo = NULL;

    // get the state object
    jsonState = json_object_get(pJSON_Obj, JSON_TX_STATE_FIELD);
    ABC_CHECK_ASSERT((jsonState && json_is_object(jsonState)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing state");

    // get the creation date
    jsonVal = json_object_get(jsonState, JSON_CREATION_DATE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing creation date");
    pInfo->timeCreation = json_integer_value(jsonVal);

    jsonVal = json_object_get(jsonState, JSON_MALLEABLE_TX_ID);
    if (jsonVal)
    {
        ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing malleable tx id");
        pInfo->szTxid = stringCopy(json_string_value(jsonVal));
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
 * Free's a tABC_Tx struct and all its elements
 */
static
void ABC_TxFreeTx(tABC_Tx *pTx)
{
    if (pTx)
    {
        ABC_FREE_STR(pTx->szNtxid);
        ABC_TxDetailsFree(pTx->pDetails);
        ABC_CLEAR_FREE(pTx->pStateInfo, sizeof(tTxStateInfo));
        ABC_CLEAR_FREE(pTx, sizeof(tABC_Tx));
    }
}

/**
 * Saves a transaction to disk
 *
 * @param pTx  Pointer to transaction data
 */
static
tABC_CC ABC_TxSaveTransaction(Wallet &self,
                              const tABC_Tx *pTx,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL(pTx->pStateInfo);
    ABC_CHECK_NULL(pTx->szNtxid);

    // create the json for the transaction
    pJSON_Root = json_object();
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_Error, "Could not create transaction JSON object");

    // set the ID
    json_object_set_new(pJSON_Root, JSON_TX_NTXID_FIELD, json_string(pTx->szNtxid));

    // set the state info
    ABC_CHECK_RET(ABC_TxEncodeTxState(pJSON_Root, pTx->pStateInfo, pError));

    // set the details
    ABC_CHECK_RET(ABC_TxDetailsEncode(pJSON_Root, pTx->pDetails, pError));

    // create the transaction directory if needed
    ABC_CHECK_NEW(fileEnsureDir(self.txDir()));

    // get the filename for this transaction
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, pTx->szNtxid, pTx->pStateInfo->bInternal, pError));

    // save out the transaction object to a file encrypted with the master key
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, toU08Buf(self.dataKey()), ABC_CryptoType_AES256, szFilename, pError));

    self.balanceDirty();

exit:
    ABC_FREE_STR(szFilename);
    if (pJSON_Root) json_decref(pJSON_Root);

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
    retVal = json_object_set_new(pJSON_State, JSON_MALLEABLE_TX_ID, json_string(pInfo->szTxid));
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

void ABC_TxFreeOutputs(tABC_TxOutput **aOutputs, unsigned int count)
{
    if ((aOutputs != NULL) && (count > 0))
    {
        for (unsigned i = 0; i < count; i++)
        {
            tABC_TxOutput *pOutput = aOutputs[i];
            if (pOutput)
            {
                ABC_FREE_STR(pOutput->szAddress);
                ABC_FREE_STR(pOutput->szTxId);
                ABC_CLEAR_FREE(pOutput, sizeof(tABC_TxOutput));
            }
        }
        ABC_CLEAR_FREE(aOutputs, sizeof(tABC_TxOutput *) * count);
    }
}

tABC_CC ABC_TxTransactionExists(Wallet &self,
                                const std::string &ntxid,
                                tABC_Tx **pTx,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    char *szFilename = NULL;

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, ntxid, true, pError));
    if (!fileExists(szFilename))
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, ntxid, false, pError));
    }

    if (fileExists(szFilename))
    {
        ABC_CHECK_RET(ABC_TxLoadTransaction(self, szFilename, pTx, pError));
    }
    else
    {
        *pTx = NULL;
    }

exit:
    ABC_FREE_STR(szFilename);

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
    size_t pos = 2, cnd = 0;
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
    int result = -1;
    size_t haystack_size;
    size_t needle_size;
    size_t m = 0, i = 0;
    int *table;

    haystack_size = strlen(haystack);
    needle_size = strlen(needle);

    if (haystack_size == 0 || needle_size == 0) {
        return 0;
    }

    ABC_ARRAY_NEW(table, needle_size, int);
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

} // namespace abcd
