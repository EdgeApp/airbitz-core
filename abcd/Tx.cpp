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
#include "bitcoin/WatcherBridge.hpp"
#include "crypto/Crypto.hpp"
#include "spend/Spend.hpp"
#include "util/Debug.hpp"
#include "util/FileIO.hpp"
#include "util/Mutex.hpp"
#include "util/Util.hpp"
#include "wallet/Details.hpp"
#include "wallet/Wallet.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <string.h>
#include <qrencode.h>
#include <wallet/wallet.hpp>
#include <unordered_map>
#include <string>

namespace abcd {

#define MIN_RECYCLABLE 5

#define TX_MAX_ADDR_ID_LENGTH                   20 // largest char count for the string version of the id number - 20 digits should handle it

#define TX_MAX_AMOUNT_LENGTH                    100 // should be max length of a bit coin amount string
#define TX_MAX_CATEGORY_LENGTH                  512

#define TX_INTERNAL_SUFFIX                      "-int.json" // the transaction was created by our direct action (i.e., send)
#define TX_EXTERNAL_SUFFIX                      "-ext.json" // the transaction was created due to events in the block-chain (usually receives)

#define ADDRESS_FILENAME_SEPARATOR              '-'
#define ADDRESS_FILENAME_SUFFIX                 ".json"
#define ADDRESS_FILENAME_MIN_LEN                8 // <id>-<public_addr>.json

#define JSON_CREATION_DATE_FIELD                "creationDate"
#define JSON_MALLEABLE_TX_ID                    "malleableTxId"
#define JSON_AMOUNT_SATOSHI_FIELD               "amountSatoshi"

#define JSON_TX_ID_FIELD                        "ntxid"
#define JSON_TX_STATE_FIELD                     "state"
#define JSON_TX_INTERNAL_FIELD                  "internal"
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
    char                *szPubAddress; // public address
    tABC_TxDetails      *pDetails;
    tTxAddressStateInfo *pStateInfo;
} tABC_TxAddress;

static tABC_CC  ABC_TxCreateNewAddress(Wallet &self, tABC_TxDetails *pDetails, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_TxCreateNewAddressForN(Wallet &self, int32_t N, tABC_Error *pError);
static tABC_CC  ABC_GetAddressFilename(Wallet &self, const char *szRequestID, char **pszFilename, tABC_Error *pError);
static tABC_CC  ABC_TxParseAddrFilename(const char *szFilename, char **pszID, char **pszPublicAddress, tABC_Error *pError);
static tABC_CC  ABC_TxSetAddressRecycle(Wallet &self, const char *szAddress, bool bRecyclable, tABC_Error *pError);
static tABC_CC  ABC_TxCheckForInternalEquivalent(const char *szFilename, bool *pbEquivalent, tABC_Error *pError);
static tABC_CC  ABC_TxGetTxTypeAndBasename(const char *szFilename, tTxType *pType, char **pszBasename, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTransactionInfo(Wallet &self, const char *szFilename, tABC_TxInfo **ppTransaction, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTxAndAppendToArray(Wallet &self, int64_t startTime, int64_t endTime, const char *szFilename, tABC_TxInfo ***paTransactions, unsigned int *pCount, tABC_Error *pError);
static tABC_CC  ABC_TxGetAddressOwed(tABC_TxAddress *pAddr, int64_t *pSatoshiBalance, tABC_Error *pError);
static tABC_CC  ABC_TxBuildFromLabel(Wallet &self, const char **pszLabel, tABC_Error *pError);
static void     ABC_TxFreeRequest(tABC_RequestInfo *pRequest);
static tABC_CC  ABC_TxCreateTxFilename(Wallet &self, char **pszFilename, const char *szTxID, bool bInternal, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTransaction(Wallet &self, const char *szFilename, tABC_Tx **ppTx, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeTxState(json_t *pJSON_Obj, tTxStateInfo **ppInfo, tABC_Error *pError);
static void     ABC_TxFreeTx(tABC_Tx *pTx);
static tABC_CC  ABC_TxSaveTransaction(Wallet &self, const tABC_Tx *pTx, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeTxState(json_t *pJSON_Obj, tTxStateInfo *pInfo, tABC_Error *pError);
static int      ABC_TxInfoPtrCompare (const void * a, const void * b);
static tABC_CC  ABC_TxLoadAddress(Wallet &self, const char *szAddressID, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_TxLoadAddressFile(Wallet &self, const char *szFilename, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo **ppState, tABC_Error *pError);
static tABC_CC  ABC_TxSaveAddress(Wallet &self, const tABC_TxAddress *pAddress, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo *pInfo, tABC_Error *pError);
static tABC_CC  ABC_TxCreateAddressFilename(Wallet &self, char **pszFilename, const tABC_TxAddress *pAddress, tABC_Error *pError);
static void     ABC_TxFreeAddress(tABC_TxAddress *pAddress);
static void     ABC_TxFreeAddressStateInfo(tTxAddressStateInfo *pInfo);
static void     ABC_TxFreeAddresses(tABC_TxAddress **aAddresses, unsigned int count);
static tABC_CC  ABC_TxGetAddresses(Wallet &self, tABC_TxAddress ***paAddresses, unsigned int *pCount, tABC_Error *pError);
static int      ABC_TxAddrPtrCompare(const void * a, const void * b);
static tABC_CC  ABC_TxLoadAddressAndAppendToArray(Wallet &self, const char *szFilename, tABC_TxAddress ***paAddresses, unsigned int *pCount, tABC_Error *pError);
static tABC_CC  ABC_TxTransactionExists(Wallet &self, const char *szID, tABC_Tx **pTx, tABC_Error *pError);
static void     ABC_TxStrTable(const char *needle, int *table);
static int      ABC_TxStrStr(const char *haystack, const char *needle, tABC_Error *pError);
static int      ABC_TxCopyOuputs(tABC_Tx *pTx, tABC_TxOutput **aOutputs, int countOutputs, tABC_Error *pError);
static tABC_CC  ABC_TxWalletOwnsAddress(Wallet &self, const char *szAddress, bool *bFound, tABC_Error *pError);
static tABC_CC  ABC_TxSaveNewTx(Wallet &self, tABC_Tx *pTx, bool bOutside, tABC_Error *pError);
static tABC_CC  ABC_TxTrashAddresses(Wallet &self, tABC_TxDetails **ppDetails, tTxAddressActivity *pActivity, tABC_TxOutput **paAddresses, unsigned int addressCount, tABC_Error *pError);
static tABC_CC  ABC_TxCalcCurrency(Wallet &self, int64_t amountSatoshi, double *pCurrency, tABC_Error *pError);

/**
 * Calculates a public address for the HD wallet main external chain.
 * @param pszPubAddress set to the newly-generated address, or set to NULL if
 * there is a math error. If that happens, add 1 to N and try again.
 * @param PrivateSeed any amount of random data to seed the generator
 * @param N the index of the key to generate
 */
static tABC_CC
ABC_BridgeGetBitcoinPubAddress(char **pszPubAddress,
                               const DataChunk &seed,
                                       int32_t N,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    libwallet::hd_private_key m(seed);
    libwallet::hd_private_key m0 = m.generate_private_key(0);
    libwallet::hd_private_key m00 = m0.generate_private_key(0);
    libwallet::hd_private_key m00n = m00.generate_private_key(N);
    if (m00n.valid())
    {
        std::string out = m00n.address().encoded();
        *pszPubAddress = stringCopy(out);
    }
    else
    {
        *pszPubAddress = nullptr;
    }

    return cc;
}

Status
txKeyTableGet(KeyTable &result, Wallet &self)
{
    libwallet::hd_private_key m(self.bitcoinKey());
    libwallet::hd_private_key m0 = m.generate_private_key(0);
    libwallet::hd_private_key m00 = m0.generate_private_key(0);

    tABC_TxAddress **aAddresses = nullptr;
    unsigned int countAddresses = 0;
    ABC_CHECK_OLD(ABC_TxGetAddresses(self, &aAddresses, &countAddresses, &error));

    KeyTable out;
    for (unsigned i = 0; i < countAddresses; i++)
    {
        libwallet::hd_private_key m00n =
            m00.generate_private_key(aAddresses[i]->seq);
        if (!m00n.valid())
            return ABC_ERROR(ABC_CC_NULLPtr, "Super-unlucky key derivation path!");

        out[m00n.address().encoded()] =
            libwallet::secret_to_wif(m00n.private_key());
    }

    ABC_TxFreeAddresses(aAddresses, countAddresses);

    result = std::move(out);
    return Status();
}

Status
txNewChangeAddress(std::string &result, Wallet &self,
    tABC_TxDetails *pDetails)
{
    AutoFree<tABC_TxAddress, ABC_TxFreeAddress> pAddress;
    ABC_CHECK_OLD(ABC_TxCreateNewAddress(self, pDetails, &pAddress.get(), &error));
    ABC_CHECK_OLD(ABC_TxSaveAddress(self, pAddress, &error));

    result = pAddress->szPubAddress;
    return Status();
}

tABC_CC ABC_TxSendComplete(Wallet &self,
                           SendInfo         *pInfo,
                           tABC_UnsavedTx   *pUtx,
                           tABC_Error       *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    tABC_Tx *pTx = structAlloc<tABC_Tx>();
    tABC_Tx *pReceiveTx = NULL;
    bool bFound = false;
    double currency;

    // Start watching all addresses incuding new change addres
    ABC_CHECK_RET(ABC_TxWatchAddresses(self, pError));

    // set the state
    pTx->pStateInfo = structAlloc<tTxStateInfo>();
    pTx->pStateInfo->timeCreation = time(NULL);
    pTx->pStateInfo->bInternal = true;
    pTx->pStateInfo->szMalleableTxId = stringCopy(pUtx->szTxMalleableId);
    // Copy outputs
    ABC_TxCopyOuputs(pTx, pUtx->aOutputs, pUtx->countOutputs, pError);
    // copy the details
    ABC_CHECK_RET(ABC_TxDetailsCopy(&(pTx->pDetails), pInfo->pDetails, pError));
    // Add in tx fees to the amount of the tx

    if (pInfo->szDestAddress)
    {
        ABC_CHECK_RET(ABC_TxWalletOwnsAddress(self, pInfo->szDestAddress, &bFound, pError));
    }
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

    ABC_CHECK_RET(ABC_TxCalcCurrency(
        self, pTx->pDetails->amountSatoshi, &currency, pError));
    pTx->pDetails->amountCurrency = currency;

    if (pTx->pDetails->amountSatoshi > 0)
        pTx->pDetails->amountSatoshi *= -1;
    if (pTx->pDetails->amountCurrency > 0)
        pTx->pDetails->amountCurrency *= -1.0;

    // Store transaction ID
    pTx->szID = stringCopy(pUtx->szTxId);

    // Save the transaction:
    ABC_CHECK_RET(ABC_TxSaveNewTx(self, pTx, false, pError));

    if (pInfo->bTransfer)
    {
        pReceiveTx = structAlloc<tABC_Tx>();
        pReceiveTx->pStateInfo = structAlloc<tTxStateInfo>();

        // set the state
        pReceiveTx->pStateInfo->timeCreation = time(NULL);
        pReceiveTx->pStateInfo->bInternal = true;
        pReceiveTx->pStateInfo->szMalleableTxId = stringCopy(pUtx->szTxMalleableId);
        // Copy outputs
        ABC_TxCopyOuputs(pReceiveTx, pUtx->aOutputs, pUtx->countOutputs, pError);
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
        pReceiveTx->szID = stringCopy(pUtx->szTxId);

        // save the transaction
        ABC_CHECK_RET(ABC_TxSaveNewTx(*pInfo->walletDest, pReceiveTx, false, pError));
    }

exit:
    ABC_TxFreeTx(pTx);
    ABC_TxFreeTx(pReceiveTx);
    return cc;
}

tABC_CC ABC_TxWalletOwnsAddress(Wallet &self,
                                const char *szAddress,
                                bool *bFound,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_TxAddress **aAddresses = NULL;
    unsigned int countAddresses = 0;

    ABC_CHECK_RET(ABC_TxGetAddresses(self, &aAddresses, &countAddresses, pError));
    *bFound = false;
    for (unsigned i = 0; i < countAddresses; ++i)
    {
        if (strcmp(szAddress, aAddresses[i]->szPubAddress) == 0)
        {
            *bFound = true;
            break;
        }
    }

exit:
    ABC_TxFreeAddresses(aAddresses, countAddresses);
    return cc;
}

tABC_CC ABC_TxWatchAddresses(Wallet &self,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    char *szPubAddress          = NULL;
    tABC_TxAddress **aAddresses = NULL;
    unsigned int countAddresses = 0;

    ABC_CHECK_RET(
        ABC_TxGetAddresses(self, &aAddresses, &countAddresses, pError));
    for (unsigned i = 0; i < countAddresses; i++)
    {
        const tABC_TxAddress *a = aAddresses[i];
        ABC_CHECK_RET(ABC_BridgeWatchAddr(self, a->szPubAddress, pError));
    }
exit:
    ABC_TxFreeAddresses(aAddresses, countAddresses);
    ABC_FREE_STR(szPubAddress);

    return cc;
}

/**
 * Handles creating or updating when we receive a transaction
 */
tABC_CC ABC_TxReceiveTransaction(Wallet &self,
                                 uint64_t amountSatoshi, uint64_t feeSatoshi,
                                 tABC_TxOutput **paInAddresses, unsigned int inAddressCount,
                                 tABC_TxOutput **paOutAddresses, unsigned int outAddressCount,
                                 const char *szTxId, const char *szMalTxId,
                                 tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                                 void *pData,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    tABC_Tx *pTx = NULL;
    double currency = 0.0;

    // Does the transaction already exist?
    ABC_TxTransactionExists(self, szTxId, &pTx, pError);
    if (pTx == NULL)
    {
        ABC_CHECK_RET(ABC_TxCalcCurrency(self, amountSatoshi, &currency, pError));

        // create a transaction
        pTx = structAlloc<tABC_Tx>();
        pTx->pStateInfo = structAlloc<tTxStateInfo>();
        pTx->pDetails = structAlloc<tABC_TxDetails>();

        pTx->pStateInfo->szMalleableTxId = stringCopy(szMalTxId);
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
        pTx->szID = stringCopy(szTxId);
        // store the input addresses
        pTx->countOutputs = inAddressCount + outAddressCount;
        ABC_ARRAY_NEW(pTx->aOutputs, pTx->countOutputs, tABC_TxOutput*);
        for (unsigned i = 0; i < inAddressCount; ++i)
        {
            ABC_DebugLog("Saving Input address: %s\n", paInAddresses[i]->szAddress);

            pTx->aOutputs[i] = structAlloc<tABC_TxOutput>();
            pTx->aOutputs[i]->szAddress = stringCopy(paInAddresses[i]->szAddress);
            pTx->aOutputs[i]->szTxId = stringCopy(paInAddresses[i]->szTxId);
            pTx->aOutputs[i]->input = paInAddresses[i]->input;
            pTx->aOutputs[i]->value = paInAddresses[i]->value;
        }
        for (unsigned i = 0; i < outAddressCount; ++i)
        {
            ABC_DebugLog("Saving Output address: %s\n", paOutAddresses[i]->szAddress);
            int newi = i + inAddressCount;
            pTx->aOutputs[newi] = structAlloc<tABC_TxOutput>();
            pTx->aOutputs[newi]->szAddress = stringCopy(paOutAddresses[i]->szAddress);
            pTx->aOutputs[newi]->szTxId = stringCopy(paOutAddresses[i]->szTxId);
            pTx->aOutputs[newi]->input = paOutAddresses[i]->input;
            pTx->aOutputs[newi]->value = paOutAddresses[i]->value;
        }

        // add the transaction to the address
        ABC_CHECK_RET(ABC_TxSaveNewTx(self, pTx, true, pError));

        // Mark the wallet cache as dirty in case the Tx wasn't included in the current balance
        self.balanceDirty();

        if (fAsyncBitCoinEventCallback)
        {
            tABC_AsyncBitCoinInfo info;
            info.pData = pData;
            info.eventType = ABC_AsyncEventType_IncomingBitCoin;
            info.szTxID = stringCopy(pTx->szID);
            info.szWalletUUID = stringCopy(self.id());
            info.szDescription = stringCopy("Received funds");
            fAsyncBitCoinEventCallback(&info);
            ABC_FREE_STR(info.szTxID);
            ABC_FREE_STR(info.szDescription);
        }
    }
    else
    {
        ABC_DebugLog("We already have %s\n", szTxId);

        // Mark the wallet cache as dirty in case the Tx wasn't included in the current balance
        self.balanceDirty();

        if (fAsyncBitCoinEventCallback)
        {
            tABC_AsyncBitCoinInfo info;
            info.pData = pData;
            info.eventType = ABC_AsyncEventType_DataSyncUpdate;
            info.szTxID = stringCopy(pTx->szID);
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
                bool bOutside,
                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tTxAddressActivity activity;
    AutoFree<tABC_TxDetails, ABC_TxDetailsFree> pDetails;

    activity.szTxID = pTx->szID;
    activity.timeCreation = pTx->pStateInfo->timeCreation;
    activity.amountSatoshi = pTx->pDetails->amountSatoshi;

    ABC_CHECK_RET(ABC_TxTrashAddresses(self, &pDetails.get(),
        &activity, pTx->aOutputs, pTx->countOutputs, pError));

    if (bOutside)
    {
        if (ABC_STRLEN(pDetails->szName) && !ABC_STRLEN(pTx->pDetails->szName))
            pTx->pDetails->szName = stringCopy(pDetails->szName);
        if (ABC_STRLEN(pDetails->szNotes) && !ABC_STRLEN(pTx->pDetails->szNotes))
            pTx->pDetails->szNotes = stringCopy(pDetails->szNotes);
        if (ABC_STRLEN(pDetails->szCategory) && !ABC_STRLEN(pTx->pDetails->szCategory))
            pTx->pDetails->szCategory = stringCopy(pDetails->szCategory);
    }
    ABC_CHECK_RET(ABC_TxSaveTransaction(self, pTx, pError));

exit:
    return cc;
}

/**
 * Marks the address as unusable and returns its metadata.
 *
 * @param ppDetails     Metadata extracted from the address database
 * @param paAddress     Addresses that will be updated
 * @param addressCount  Number of address in paAddress
 */
static
tABC_CC ABC_TxTrashAddresses(Wallet &self,
                             tABC_TxDetails **ppDetails,
                             tTxAddressActivity *pActivity,
                             tABC_TxOutput **paAddresses,
                             unsigned int addressCount,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_TxAddress *pAddress = NULL;

    unsigned int localCount = 0;
    tABC_TxAddress **pInternalAddress = NULL;
    std::unordered_map<std::string, sABC_TxAddress*> addrMap;

    // Create a more efficient structure to query
    ABC_CHECK_RET(ABC_TxGetAddresses(self, &pInternalAddress, &localCount, pError));
    for (unsigned i = 0; i < localCount; ++i)
    {
        std::string addr(pInternalAddress[i]->szPubAddress);
        addrMap[addr] = pInternalAddress[i];
    }

    *ppDetails = nullptr;
    for (unsigned i = 0; i < addressCount; ++i)
    {
        if (paAddresses[i]->input)
            continue;

        std::string addr(paAddresses[i]->szAddress);
        if (addrMap.find(addr) == addrMap.end())
            continue;

        pAddress = addrMap[addr];
        if (pAddress)
        {
            unsigned int countActivities = pAddress->pStateInfo->countActivities;
            tTxAddressActivity *aActivities = pAddress->pStateInfo->aActivities;

            // grow the array:
            ABC_ARRAY_RESIZE(aActivities, countActivities + 1, tTxAddressActivity);

            // fill in the new entry:
            aActivities[countActivities].szTxID = stringCopy(pActivity->szTxID);
            aActivities[countActivities].timeCreation = pActivity->timeCreation;
            aActivities[countActivities].amountSatoshi = pActivity->amountSatoshi;

            // save the array:
            pAddress->pStateInfo->countActivities = countActivities + 1;
            pAddress->pStateInfo->aActivities = aActivities;

            // Save the transaction:
            pAddress->pStateInfo->bRecycleable = false;
            ABC_CHECK_RET(ABC_TxSaveAddress(self, pAddress, pError));

            // Return our details:
            ABC_TxDetailsFree(*ppDetails);
            ABC_CHECK_RET(ABC_TxDetailsCopy(ppDetails, pAddress->pDetails, pError));
        }
        pAddress = NULL;
    }

exit:
    ABC_TxFreeAddresses(pInternalAddress, localCount);

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
 * Creates a receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param pDetails      Pointer to transaction details
 * @param pszRequestID  Pointer to store allocated ID for this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxCreateReceiveRequest(Wallet &self,
                                   tABC_TxDetails *pDetails,
                                   char **pszRequestID,
                                   bool bTransfer,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    tABC_TxAddress *pAddress = NULL;

    *pszRequestID = NULL;

    // get a new address (re-using a recycleable if we can)
    ABC_CHECK_RET(ABC_TxCreateNewAddress(self, pDetails, &pAddress, pError));

    // save out this address
    ABC_CHECK_RET(ABC_TxSaveAddress(self, pAddress, pError));

    // set the id for the caller
    *pszRequestID = stringCopy(std::to_string(pAddress->seq));

    // Watch this new address
    ABC_CHECK_RET(ABC_TxWatchAddresses(self, pError));

exit:
    ABC_TxFreeAddress(pAddress);

    return cc;
}

tABC_CC ABC_TxCreateInitialAddresses(Wallet &self,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoFree<tABC_TxDetails, ABC_TxDetailsFree>
        pDetails(structAlloc<tABC_TxDetails>());
    pDetails->szName = stringCopy("");
    pDetails->szCategory = stringCopy("");
    pDetails->szNotes = stringCopy("");
    pDetails->attributes = 0x0;
    pDetails->bizId = 0;

    ABC_CHECK_RET(ABC_TxCreateNewAddress(self, pDetails, NULL, pError));

exit:
    return cc;
}

/**
 * Creates a new address.
 * First looks to see if we can recycle one, if we can, that is the address returned.
 * This new address is not saved to the file system, the caller must make sure it is saved
 * if they want it persisted.
 *
 * @param pDetails      Pointer to transaction details to be used for the new address
 *                      (note: a copy of these are made so the caller can do whatever they want
 *                       with the pointer once the call is complete)
 * @param ppAddress     Location to store pointer to allocated address
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxCreateNewAddress(Wallet &self,
                               tABC_TxDetails *pDetails,
                               tABC_TxAddress **ppAddress,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    tABC_TxAddress **aAddresses = NULL;
    unsigned int countAddresses = 0;
    tABC_TxAddress *pAddress = NULL;
    int64_t N = -1;
    unsigned recyclable = 0;

    // first look for an existing address that we can re-use

    // load addresses
    ABC_CHECK_RET(ABC_TxGetAddresses(self, &aAddresses, &countAddresses, pError));

    // search through all of the addresses, get the highest N and check for one with the recycleable bit set
    for (unsigned i = 0; i < countAddresses; i++)
    {
        // if this is the highest seq number
        if (aAddresses[i]->seq > N)
        {
            N = aAddresses[i]->seq;
        }

        // if we don't have an address yet and this one is available
        ABC_CHECK_NULL(aAddresses[i]->pStateInfo);
        if (aAddresses[i]->pStateInfo->bRecycleable
                && aAddresses[i]->pStateInfo->countActivities == 0)
        {
            recyclable++;
            if (pAddress == NULL)
            {
                char *szRegenAddress = NULL;
                ABC_CHECK_RET(ABC_BridgeGetBitcoinPubAddress(&szRegenAddress, self.bitcoinKey(), aAddresses[i]->seq, pError));

                if (strncmp(aAddresses[i]->szPubAddress, szRegenAddress, strlen(aAddresses[i]->szPubAddress)) == 0)
                {
                    // set it to NULL so we don't free it as part of the array
                    // free, we will be sending this back to the caller
                    pAddress = aAddresses[i];
                    aAddresses[i] = NULL;
                    recyclable--;
                }
                else
                {
                    ABC_DebugLog("********************************\n");
                    ABC_DebugLog("Address Corrupt\nInitially: %s, Now: %s\nSeq: %d",
                                    aAddresses[i]->szPubAddress,
                                    szRegenAddress,
                                    aAddresses[i]->seq);
                    ABC_DebugLog("********************************\n");
                }
                ABC_FREE_STR(szRegenAddress);
            }
        }
    }
    // Create a new address at N
    if (recyclable <= MIN_RECYCLABLE)
    {
        for (unsigned i = 0; i < MIN_RECYCLABLE - recyclable; ++i)
        {
            ABC_CHECK_RET(ABC_TxCreateNewAddressForN(self, N + i, pError));
        }
    }

    // Does the caller want a result?
    if (ppAddress)
    {
        // Did we find an address to use?
        ABC_CHECK_ASSERT(pAddress != NULL, ABC_CC_NoAvailableAddress, "Unable to locate a non-corrupt address.");

        // free state and details as we will be setting them to new data below
        ABC_TxFreeAddressStateInfo(pAddress->pStateInfo);
        pAddress->pStateInfo = NULL;
        ABC_TxDetailsFree(pAddress->pDetails);
        pAddress->pDetails = NULL;

        // copy over the info we were given
        ABC_CHECK_RET(ABC_TxDetailsCopy(&(pAddress->pDetails), pDetails, pError));

        // create the state info
        pAddress->pStateInfo = structAlloc<tTxAddressStateInfo>();
        pAddress->pStateInfo->bRecycleable = true;
        pAddress->pStateInfo->countActivities = 0;
        pAddress->pStateInfo->aActivities = NULL;
        pAddress->pStateInfo->timeCreation = time(NULL);

        // assigned final address
        *ppAddress = pAddress;
        pAddress = NULL;
    }
exit:
    ABC_TxFreeAddresses(aAddresses, countAddresses);
    ABC_TxFreeAddress(pAddress);

    return cc;
}

static
tABC_CC ABC_TxCreateNewAddressForN(Wallet &self, int32_t N, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    tABC_TxAddress *pAddress = structAlloc<tABC_TxAddress>();

    // generate the public address
    pAddress->szPubAddress = NULL;
    pAddress->seq = N;
    do
    {
        // move to the next sequence number
        pAddress->seq++;

        // Get the public address for our sequence (it can return NULL, if it is invalid)
        ABC_CHECK_RET(ABC_BridgeGetBitcoinPubAddress(&(pAddress->szPubAddress), self.bitcoinKey(), pAddress->seq, pError));
    } while (pAddress->szPubAddress == NULL);

    pAddress->pStateInfo = structAlloc<tTxAddressStateInfo>();
    pAddress->pStateInfo->bRecycleable = true;
    pAddress->pStateInfo->countActivities = 0;
    pAddress->pStateInfo->aActivities = NULL;
    pAddress->pStateInfo->timeCreation = time(NULL);

    pAddress->pDetails = structAlloc<tABC_TxDetails>();
    pAddress->pDetails->szName = stringCopy("");
    pAddress->pDetails->szCategory = stringCopy("");
    pAddress->pDetails->szNotes = stringCopy("");
    pAddress->pDetails->attributes = 0x0;
    pAddress->pDetails->bizId = 0;
    pAddress->pDetails->amountSatoshi = 0;
    pAddress->pDetails->amountCurrency = 0;
    pAddress->pDetails->amountFeesAirbitzSatoshi = 0;
    pAddress->pDetails->amountFeesMinersSatoshi = 0;

    // Save the new Address
    ABC_CHECK_RET(ABC_TxSaveAddress(self, pAddress, pError));
exit:
    ABC_TxFreeAddress(pAddress);

    return cc;
}

/**
 * Modifies a previously created receive request.
 * Note: the previous details will be free'ed so if the user is using the previous details for this request
 * they should not assume they will be valid after this call.
 *
 * @param szRequestID   ID of this request
 * @param pDetails      Pointer to transaction details
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxModifyReceiveRequest(Wallet &self,
                                   const char *szRequestID,
                                   tABC_TxDetails *pDetails,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    AutoFree<tABC_TxAddress, ABC_TxFreeAddress> pAddress;
    tABC_TxDetails *pNewDetails = NULL;

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddress(self, szRequestID, &pAddress.get(), pError));

    // copy the new details
    ABC_CHECK_RET(ABC_TxDetailsCopy(&pNewDetails, pDetails, pError));

    // free the old details on this address
    ABC_TxDetailsFree(pAddress->pDetails);

    // set the new details
    pAddress->pDetails = pNewDetails;
    pNewDetails = NULL;

    // write out the address
    ABC_CHECK_RET(ABC_TxSaveAddress(self, pAddress, pError));

exit:
    ABC_TxDetailsFree(pNewDetails);

    return cc;
}

/**
 * Gets the filename for a given address based upon the address id
 *
 * @param szAddressID   ID of this address
 * @param pszFilename   Address to store pointer to filename
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_GetAddressFilename(Wallet &self,
                               const char *szAddressID,
                               char **pszFilename,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoFileLock lock(gFileMutex); // We are iterating over the filesystem

    std::string addressDir = self.addressDir();
    tABC_FileIOList *pFileList = NULL;
    char *szID = NULL;

    ABC_CHECK_NULL(szAddressID);
    ABC_CHECK_ASSERT(strlen(szAddressID) > 0, ABC_CC_Error, "No address UUID provided");
    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;

    // Make sure there is an addresses directory
    ABC_CHECK_ASSERT(fileExists(addressDir), ABC_CC_Error, "No existing requests/addresses");

    // get all the files in the address directory
    ABC_FileIOCreateFileList(&pFileList, addressDir.c_str(), NULL);
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
                *pszFilename = stringCopy(pFileList->apFiles[i]->szName);
                break;
            }
        }
    }
    ABC_CHECK_ASSERT(*pszFilename, ABC_CC_Error, "Address not found");

exit:
    ABC_FREE_STR(szID);
    ABC_FileIOFreeFileList(pFileList);

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
            for (size_t i = 0; i < strlen(szFilename); i++)
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
                    ABC_STR_NEW(*pszID, nPosSeparator + 1);
                    strncpy(*pszID, szFilename, nPosSeparator);
                }

                if (pszPublicAddress != NULL)
                {
                    *pszPublicAddress = stringCopy(&(szFilename[nPosSeparator + 1]));
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
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxFinalizeReceiveRequest(Wallet &self,
                                     const char *szRequestID,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    // set the recycle bool to false (not that the request is actually an address internally)
    ABC_CHECK_RET(ABC_TxSetAddressRecycle(self, szRequestID, false, pError));

exit:

    return cc;
}

/**
 * Cancels a previously created receive request.
 * This is done by setting the recycle bit to true so that the address can be used again.
 *
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxCancelReceiveRequest(Wallet &self,
                                   const char *szRequestID,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    // set the recycle bool to true (not that the request is actually an address internally)
    ABC_CHECK_RET(ABC_TxSetAddressRecycle(self, szRequestID, true, pError));

exit:

    return cc;
}

/**
 * Sets the recycle status on an address as specified
 *
 * @param szAddress     ID of the address
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxSetAddressRecycle(Wallet &self,
                                const char *szAddress,
                                bool bRecyclable,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    AutoFree<tABC_TxAddress, ABC_TxFreeAddress> pAddress;

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddress(self, szAddress, &pAddress.get(), pError));

    // if it isn't already set as required
    ABC_CHECK_NULL(pAddress->pStateInfo);
    if (pAddress->pStateInfo->bRecycleable != bRecyclable)
    {
        // change the recycle boolean
        ABC_CHECK_NULL(pAddress->pStateInfo);
        pAddress->pStateInfo->bRecycleable = bRecyclable;

        // write out the address
        ABC_CHECK_RET(ABC_TxSaveAddress(self, pAddress, pError));
    }

exit:
    return cc;
}

/**
 * Generate the QR code for a previously created receive request.
 *
 * @param szRequestID   ID of this request
 * @param pszURI        Pointer to string to store URI(optional)
 * @param paData        Pointer to store array of data bytes (0x0 white, 0x1 black)
 * @param pWidth        Pointer to store width of image (image will be square)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGenerateRequestQRCode(Wallet &self,
                                    const char *szRequestID,
                                    char **pszURI,
                                    unsigned char **paData,
                                    unsigned int *pWidth,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    tABC_TxAddress *pAddress = NULL;
    QRcode *qr = NULL;
    unsigned char *aData = NULL;
    unsigned int length = 0;
    char *szURI = NULL;

    // load the request/address
    ABC_CHECK_RET(ABC_TxLoadAddress(self, szRequestID, &pAddress, pError));
    ABC_CHECK_NULL(pAddress->pDetails);

    // Get the URL string for this info
    tABC_BitcoinURIInfo infoURI;
    memset(&infoURI, 0, sizeof(tABC_BitcoinURIInfo));
    infoURI.amountSatoshi = pAddress->pDetails->amountSatoshi;
    infoURI.szAddress = pAddress->szPubAddress;

    // Set the label if there is one
    ABC_CHECK_RET(ABC_TxBuildFromLabel(self, &(infoURI.szLabel), pError));

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
    ABC_ARRAY_NEW(aData, length, unsigned char);
    for (unsigned i = 0; i < length; i++)
    {
        aData[i] = qr->data[i] & 0x1;
    }
    *pWidth = qr->width;
    *paData = aData;
    aData = NULL;

    if (pszURI != NULL)
    {
        *pszURI = stringCopy(szURI);
    }

exit:
    ABC_TxFreeAddress(pAddress);
    ABC_FREE_STR(szURI);
    QRcode_free(qr);
    ABC_CLEAR_FREE(aData, length);

    return cc;
}

/**
 * Get the specified transactions.
 *
 * @param szID              ID of the transaction
 * @param ppTransaction     Location to store allocated transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetTransaction(Wallet &self,
                             const char *szID,
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
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, true, pError));
    if (!fileExists(szFilename))
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, false, pError));
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
    char satoshi[15];
    char currency[15];

    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    ABC_CHECK_NULL(paTransactions);
    *paTransactions = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    ABC_TxGetTransactions(self, ABC_GET_TX_ALL_TIMES, ABC_GET_TX_ALL_TIMES,
                          &aTransactions, &count, pError);
    ABC_ARRAY_NEW(aSearchTransactions, count, tABC_TxInfo*);
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
            ABC_CHECK_RET(ABC_FileIODeleteFile(szFilename, pError));

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
 *
 * @param szID              ID of the transaction
 * @param pDetails          Details for the transaction
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxSetTransactionDetails(Wallet &self,
                                    const char *szID,
                                    tABC_TxDetails *pDetails,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, true, pError));
    if (!fileExists(szFilename))
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, false, pError));
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
 *
 * @param szID              ID of the transaction
 * @param ppDetails         Location to store allocated details for the transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetTransactionDetails(Wallet &self,
                                    const char *szID,
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
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, true, pError));
    if (!fileExists(szFilename))
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, false, pError));
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
 * Gets the bit coin public address for a specified request
 *
 * @param szRequestID       ID of request
 * @param pszAddress        Location to store allocated address string (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetRequestAddress(Wallet &self,
                                const char *szRequestID,
                                char **pszAddress,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    AutoFree<tABC_TxAddress, ABC_TxFreeAddress> pAddress;

    ABC_CHECK_RET(ABC_TxLoadAddress(self, szRequestID, &pAddress.get(), pError));
    *pszAddress = stringCopy(pAddress->szPubAddress);

exit:
    return cc;
}

/**
 * Gets the pending requests associated with the given wallet.
 *
 * @param paTransactions    Pointer to store array of requests info pointers
 * @param pCount            Pointer to store number of requests
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetPendingRequests(Wallet &self,
                                 tABC_RequestInfo ***paRequests,
                                 unsigned int *pCount,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    tABC_TxAddress **aAddresses = NULL;
    unsigned int count = 0;
    tABC_RequestInfo **aRequests = NULL;
    unsigned int countPending = 0;
    tABC_RequestInfo *pRequest = NULL;

    *paRequests = NULL;
    *pCount = 0;

    // start by retrieving all address for this wallet
    ABC_CHECK_RET(ABC_TxGetAddresses(self, &aAddresses, &count, pError));

    // if there are any addresses
    if (count > 0)
    {
        // walk through all the addresses looking for those with outstanding balances
        for (unsigned i = 0; i < count; i++)
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
                if (!pState->bRecycleable)
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
                            pRequest = structAlloc<tABC_RequestInfo>();
                            pRequest->szID = stringCopy(std::to_string(pAddr->seq));
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
                                ABC_ARRAY_RESIZE(aRequests, countPending, tABC_RequestInfo*);
                            }
                            else
                            {
                                ABC_ARRAY_NEW(aRequests, 1, tABC_RequestInfo*);
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

    int64_t satoshiOwed = 0;
    tTxAddressStateInfo *pState = NULL;
    tABC_TxDetails  *pDetails = NULL;

    ABC_CHECK_NULL(pSatoshiOwed);
    *pSatoshiOwed = 0;
    ABC_CHECK_NULL(pAddr);
    pDetails = pAddr->pDetails;
    ABC_CHECK_NULL(pDetails);
    pState = pAddr->pStateInfo;
    ABC_CHECK_NULL(pState);

    // start with the amount requested
    satoshiOwed = pDetails->amountSatoshi;

    // if any activities have occured on this address
    if ((pState->aActivities != NULL) && (pState->countActivities > 0))
    {
        for (unsigned i = 0; i < pState->countActivities; i++)
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
 * Create a label based off the user settings
 *
 * @param pszLabel       The label will be returned in this parameter
 * @param pError         A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxBuildFromLabel(Wallet &self,
                             const char **pszLabel, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoFree<tABC_AccountSettings, ABC_FreeAccountSettings> pSettings;

    ABC_CHECK_NULL(pszLabel);
    *pszLabel = NULL;

    ABC_CHECK_RET(ABC_AccountSettingsLoad(self.account, &pSettings.get(), pError));

    if (pSettings->bNameOnPayments && pSettings->szFullName)
    {
        *pszLabel = stringCopy(pSettings->szFullName);
    }

exit:
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
        ABC_TxDetailsFree(pRequest->pDetails);

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
        for (unsigned i = 0; i < count; i++)
        {
            ABC_TxFreeRequest(aRequests[i]);
        }

        ABC_FREE(aRequests);
    }
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
                                   const char *txId,
                                   const char *malTxId,
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
    pTx->szID = stringCopy(txId);
    pTx->pStateInfo->szMalleableTxId = stringCopy(malTxId);

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
tABC_CC ABC_TxCreateTxFilename(Wallet &self, char **pszFilename, const char *szTxID, bool bInternal, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string path = self.txDir() + cryptoFilename(self.dataKey(), szTxID) +
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
    jsonVal = json_object_get(pJSON_Root, JSON_TX_ID_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing id");
    pTx->szID = stringCopy(json_string_value(jsonVal));

    // get the state object
    ABC_CHECK_RET(ABC_TxDecodeTxState(pJSON_Root, &(pTx->pStateInfo), pError));

    // get the details object
    ABC_CHECK_RET(ABC_TxDetailsDecode(pJSON_Root, &(pTx->pDetails), pError));

    // get advanced details
    ABC_CHECK_RET(
        ABC_BridgeTxDetails(self, pTx->pStateInfo->szMalleableTxId,
                            &(pTx->aOutputs), &(pTx->countOutputs),
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
        pInfo->szMalleableTxId = stringCopy(json_string_value(jsonVal));
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
        ABC_FREE_STR(pTx->szID);
        ABC_TxDetailsFree(pTx->pDetails);
        ABC_CLEAR_FREE(pTx->pStateInfo, sizeof(tTxStateInfo));
        ABC_TxFreeOutputs(pTx->aOutputs, pTx->countOutputs);
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
    int e;

    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;
    json_t *pJSON_OutputArray = NULL;
    json_t **ppJSON_Output = NULL;

    ABC_CHECK_NULL(pTx->pStateInfo);
    ABC_CHECK_NULL(pTx->szID);
    ABC_CHECK_ASSERT(strlen(pTx->szID) > 0, ABC_CC_Error, "No transaction ID provided");

    // create the json for the transaction
    pJSON_Root = json_object();
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_Error, "Could not create transaction JSON object");

    // set the ID
    json_object_set_new(pJSON_Root, JSON_TX_ID_FIELD, json_string(pTx->szID));

    // set the state info
    ABC_CHECK_RET(ABC_TxEncodeTxState(pJSON_Root, pTx->pStateInfo, pError));

    // set the details
    ABC_CHECK_RET(ABC_TxDetailsEncode(pJSON_Root, pTx->pDetails, pError));

    // create the addresses array object
    pJSON_OutputArray = json_array();

    // if there are any addresses
    if ((pTx->countOutputs > 0) && (pTx->aOutputs != NULL))
    {
        ABC_ARRAY_NEW(ppJSON_Output, pTx->countOutputs, json_t*);
        for (unsigned i = 0; i < pTx->countOutputs; i++)
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
    e = json_object_set(pJSON_Root, JSON_TX_OUTPUTS_FIELD, pJSON_OutputArray);
    ABC_CHECK_ASSERT(e == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // create the transaction directory if needed
    ABC_CHECK_NEW(fileEnsureDir(self.txDir()));

    // get the filename for this transaction
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, pTx->szID, pTx->pStateInfo->bInternal, pError));

    // save out the transaction object to a file encrypted with the master key
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, toU08Buf(self.dataKey()), ABC_CryptoType_AES256, szFilename, pError));

    self.balanceDirty();

exit:
    ABC_FREE_STR(szFilename);
    ABC_CLEAR_FREE(ppJSON_Output, sizeof(json_t *) * pTx->countOutputs);
    if (pJSON_Root) json_decref(pJSON_Root);
    if (pJSON_OutputArray) json_decref(pJSON_OutputArray);

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
 * @param szAddressID   ID of the address
 * @param ppAddress     Pointer to location to store allocated address info
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadAddress(Wallet &self,
                          const char *szAddressID,
                          tABC_TxAddress **ppAddress,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFile = NULL;
    std::string path;

    // get the filename for this address
    ABC_CHECK_RET(ABC_GetAddressFilename(self, szAddressID, &szFile, pError));
    path = self.addressDir() + szFile;

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(self, path.c_str(), ppAddress, pError));

exit:
    ABC_FREE_STR(szFile);

    return cc;
}

/**
 * Loads an address from disk given filename (complete path)
 *
 * @param ppAddress  Pointer to location to hold allocated address
 *                   (it is the callers responsiblity to free this address)
 */
static
tABC_CC ABC_TxLoadAddressFile(Wallet &self,
                              const char *szFilename,
                              tABC_TxAddress **ppAddress,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    json_t *pJSON_Root = NULL;
    tABC_TxAddress *pAddress = structAlloc<tABC_TxAddress>();
    json_t *jsonVal = NULL;

    *ppAddress = NULL;

    // make sure the addresss exists
    ABC_CHECK_ASSERT(fileExists(szFilename), ABC_CC_NoRequest, "Request address does not exist");

    // load the json object (load file, decrypt it, create json object
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, toU08Buf(self.dataKey()), &pJSON_Root, pError));

    // get the seq and id
    jsonVal = json_object_get(pJSON_Root, JSON_ADDR_SEQ_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing seq");
    pAddress->seq = (uint32_t)json_integer_value(jsonVal);

    // get the public address field
    jsonVal = json_object_get(pJSON_Root, JSON_ADDR_ADDRESS_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing address");
    pAddress->szPubAddress = stringCopy(json_string_value(jsonVal));

    // get the state object
    ABC_CHECK_RET(ABC_TxDecodeAddressStateInfo(pJSON_Root, &(pAddress->pStateInfo), pError));

    // get the details object
    ABC_CHECK_RET(ABC_TxDetailsDecode(pJSON_Root, &(pAddress->pDetails), pError));

    // assign final result
    *ppAddress = pAddress;
    pAddress = NULL;

exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    ABC_TxFreeAddress(pAddress);

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

    tTxAddressStateInfo *pState = structAlloc<tTxAddressStateInfo>();
    json_t *jsonState = NULL;
    json_t *jsonVal = NULL;
    json_t *jsonActivity = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppState);
    *ppState = NULL;

    // get the state object
    jsonState = json_object_get(pJSON_Obj, JSON_ADDR_STATE_FIELD);
    ABC_CHECK_ASSERT((jsonState && json_is_object(jsonState)), ABC_CC_JSONError, "Error parsing JSON address package - missing state info");

    // get the creation date
    jsonVal = json_object_get(jsonState, JSON_CREATION_DATE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing creation date");
    pState->timeCreation = json_integer_value(jsonVal);

    // get the internal boolean
    jsonVal = json_object_get(jsonState, JSON_ADDR_RECYCLEABLE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_boolean(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing recycleable boolean");
    pState->bRecycleable = json_is_true(jsonVal) ? true : false;

    // get the activity array (if it exists)
    jsonActivity = json_object_get(jsonState, JSON_ADDR_ACTIVITY_FIELD);
    if (jsonActivity)
    {
        ABC_CHECK_ASSERT(json_is_array(jsonActivity), ABC_CC_JSONError, "Error parsing JSON address package - missing activity array");

        // get the number of elements in the array
        pState->countActivities = (int) json_array_size(jsonActivity);

        if (pState->countActivities > 0)
        {
            ABC_ARRAY_NEW(pState->aActivities, pState->countActivities, tTxAddressActivity);

            for (unsigned i = 0; i < pState->countActivities; i++)
            {
                json_t *pJSON_Elem = json_array_get(jsonActivity, i);
                ABC_CHECK_ASSERT((pJSON_Elem && json_is_object(pJSON_Elem)), ABC_CC_JSONError, "Error parsing JSON address package - missing activity array element");

                // get the tx id
                jsonVal = json_object_get(pJSON_Elem, JSON_TX_ID_FIELD);
                ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing activity txid");
                pState->aActivities[i].szTxID = stringCopy(json_string_value(jsonVal));

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
tABC_CC ABC_TxSaveAddress(Wallet &self,
                          const tABC_TxAddress *pAddress,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL(pAddress->pStateInfo);

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
    ABC_CHECK_RET(ABC_TxDetailsEncode(pJSON_Root, pAddress->pDetails, pError));

    // create the address directory if needed
    ABC_CHECK_NEW(fileEnsureDir(self.addressDir()));

    // create the filename for this transaction
    ABC_CHECK_RET(ABC_TxCreateAddressFilename(self, &szFilename, pAddress, pError));

    // save out the transaction object to a file encrypted with the master key
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, toU08Buf(self.dataKey()), ABC_CryptoType_AES256, szFilename, pError));

exit:
    ABC_FREE_STR(szFilename);
    if (pJSON_Root) json_decref(pJSON_Root);

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
        for (unsigned i = 0; i < pInfo->countActivities; i++)
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
tABC_CC ABC_TxCreateAddressFilename(Wallet &self, char **pszFilename, const tABC_TxAddress *pAddress, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string path = self.addressDir() +
        std::to_string(pAddress->seq) + "-" +
        cryptoFilename(self.dataKey(), pAddress->szPubAddress) + ".json";;

    *pszFilename = stringCopy(path);

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
        ABC_FREE_STR(pAddress->szPubAddress);
        ABC_TxDetailsFree(pAddress->pDetails);
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
            for (unsigned i = 0; i < pInfo->countActivities; i++)
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
void ABC_TxFreeAddresses(tABC_TxAddress **aAddresses, unsigned int count)
{
    if ((aAddresses != NULL) && (count > 0))
    {
        for (unsigned i = 0; i < count; i++)
        {
            ABC_TxFreeAddress(aAddresses[i]);
        }

        ABC_CLEAR_FREE(aAddresses, sizeof(tABC_TxAddress *) * count);
    }
}

void ABC_UnsavedTxFree(tABC_UnsavedTx *pUtx)
{
    if (pUtx)
    {
        ABC_FREE_STR(pUtx->szTxId);
        ABC_FREE_STR(pUtx->szTxMalleableId);
        ABC_TxFreeOutputs(pUtx->aOutputs, pUtx->countOutputs);

        ABC_CLEAR_FREE(pUtx, sizeof(tABC_UnsavedTx));
    }
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

/**
 * Gets the addresses associated with the given wallet.
 *
 * @param paAddresses       Pointer to store array of addresses info pointers
 * @param pCount            Pointer to store number of addresses
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxGetAddresses(Wallet &self,
                           tABC_TxAddress ***paAddresses,
                           unsigned int *pCount,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    AutoFileLock fileLock(gFileMutex); // We are iterating over the filesystem

    std::string addressDir = self.addressDir();
    tABC_FileIOList *pFileList = NULL;
    tABC_TxAddress **aAddresses = NULL;
    unsigned int count = 0;

    *paAddresses = NULL;
    *pCount = 0;

    // if there is a address directory
    if (fileExists(addressDir))
    {
        // get all the files in the address directory
        ABC_FileIOCreateFileList(&pFileList, addressDir.c_str(), NULL);
        for (int i = 0; i < pFileList->nCount; i++)
        {
            // if this file is a normal file
            if (pFileList->apFiles[i]->type == ABC_FileIOFileType_Regular)
            {
                std::string path = addressDir + pFileList->apFiles[i]->szName;

                // add this address to the array
                ABC_CHECK_RET(ABC_TxLoadAddressAndAppendToArray(self, path.c_str(), &aAddresses, &count, pError));
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
    ABC_FileIOFreeFileList(pFileList);
    ABC_TxFreeAddresses(aAddresses, count);

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
 * @param szFilename        Filename of address
 * @param paAddress         Pointer to array into which the address will be added
 * @param pCount            Pointer to store number of address (will be updated)
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadAddressAndAppendToArray(Wallet &self,
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

    // hold on to current values
    count = *pCount;
    aAddresses = *paAddresses;

    // load the address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(self, szFilename, &pAddress, pError));

    // create space for new entry
    if (aAddresses == NULL)
    {
        ABC_ARRAY_NEW(aAddresses, 1, tABC_TxAddress*);
        count = 1;
    }
    else
    {
        count++;
        ABC_ARRAY_RESIZE(aAddresses, count, tABC_TxAddress*);
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

tABC_CC ABC_TxTransactionExists(Wallet &self,
                                const char *szID,
                                tABC_Tx **pTx,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    char *szFilename = NULL;

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, true, pError));
    if (!fileExists(szFilename))
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, false, pError));
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
        ABC_ARRAY_NEW(pTx->aOutputs, pTx->countOutputs, tABC_TxOutput*);
        for (i = 0; i < countOutputs; ++i)
        {
            ABC_DebugLog("Saving Outputs: %s\n", aOutputs[i]->szAddress);
            pTx->aOutputs[i] = structAlloc<tABC_TxOutput>();
            pTx->aOutputs[i]->szAddress = stringCopy(aOutputs[i]->szAddress);
            pTx->aOutputs[i]->szTxId = stringCopy(aOutputs[i]->szTxId);
            pTx->aOutputs[i]->input = aOutputs[i]->input;
            pTx->aOutputs[i]->value = aOutputs[i]->value;
        }
    }
exit:
    return cc;
}

} // namespace abcd
