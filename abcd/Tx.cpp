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
#include "Wallet.hpp"
#include "account/Account.hpp"
#include "account/AccountSettings.hpp"
#include "bitcoin/Text.hpp"
#include "bitcoin/Watcher.hpp"
#include "bitcoin/WatcherBridge.hpp"
#include "crypto/Crypto.hpp"
#include "exchange/Exchange.hpp"
#include "spend/Broadcast.hpp"
#include "spend/Inputs.hpp"
#include "util/Debug.hpp"
#include "util/FileIO.hpp"
#include "util/Mutex.hpp"
#include "util/Util.hpp"
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

static tABC_CC  ABC_TxCreateNewAddress(tABC_WalletID self, tABC_TxDetails *pDetails, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_TxCreateNewAddressForN(tABC_WalletID self, int32_t N, tABC_Error *pError);
static tABC_CC  ABC_GetAddressFilename(const char *szWalletUUID, const char *szRequestID, char **pszFilename, tABC_Error *pError);
static tABC_CC  ABC_TxParseAddrFilename(const char *szFilename, char **pszID, char **pszPublicAddress, tABC_Error *pError);
static tABC_CC  ABC_TxSetAddressRecycle(tABC_WalletID self, const char *szAddress, bool bRecyclable, tABC_Error *pError);
static tABC_CC  ABC_TxCheckForInternalEquivalent(const char *szFilename, bool *pbEquivalent, tABC_Error *pError);
static tABC_CC  ABC_TxGetTxTypeAndBasename(const char *szFilename, tTxType *pType, char **pszBasename, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTransactionInfo(tABC_WalletID self, const char *szFilename, tABC_TxInfo **ppTransaction, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTxAndAppendToArray(tABC_WalletID self, int64_t startTime, int64_t endTime, const char *szFilename, tABC_TxInfo ***paTransactions, unsigned int *pCount, tABC_Error *pError);
static tABC_CC  ABC_TxGetAddressOwed(tABC_TxAddress *pAddr, int64_t *pSatoshiBalance, tABC_Error *pError);
static tABC_CC  ABC_TxBuildFromLabel(tABC_WalletID self, char **pszLabel, tABC_Error *pError);
static void     ABC_TxFreeRequest(tABC_RequestInfo *pRequest);
static tABC_CC  ABC_TxCreateTxFilename(tABC_WalletID self, char **pszFilename, const char *szTxID, bool bInternal, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTransaction(tABC_WalletID self, const char *szFilename, tABC_Tx **ppTx, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeTxState(json_t *pJSON_Obj, tTxStateInfo **ppInfo, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails **ppDetails, tABC_Error *pError);
static void     ABC_TxFreeTx(tABC_Tx *pTx);
static tABC_CC  ABC_TxSaveTransaction(tABC_WalletID self, const tABC_Tx *pTx, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeTxState(json_t *pJSON_Obj, tTxStateInfo *pInfo, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails *pDetails, tABC_Error *pError);
static int      ABC_TxInfoPtrCompare (const void * a, const void * b);
static tABC_CC  ABC_TxLoadAddress(tABC_WalletID self, const char *szAddressID, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_TxLoadAddressFile(tABC_WalletID self, const char *szFilename, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo **ppState, tABC_Error *pError);
static tABC_CC  ABC_TxSaveAddress(tABC_WalletID self, const tABC_TxAddress *pAddress, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo *pInfo, tABC_Error *pError);
static tABC_CC  ABC_TxCreateAddressFilename(tABC_WalletID self, char **pszFilename, const tABC_TxAddress *pAddress, tABC_Error *pError);
static void     ABC_TxFreeAddress(tABC_TxAddress *pAddress);
static void     ABC_TxFreeAddressStateInfo(tTxAddressStateInfo *pInfo);
static void     ABC_TxFreeAddresses(tABC_TxAddress **aAddresses, unsigned int count);
static tABC_CC  ABC_TxGetAddresses(tABC_WalletID self, tABC_TxAddress ***paAddresses, unsigned int *pCount, tABC_Error *pError);
static int      ABC_TxAddrPtrCompare(const void * a, const void * b);
static tABC_CC  ABC_TxLoadAddressAndAppendToArray(tABC_WalletID self, const char *szFilename, tABC_TxAddress ***paAddresses, unsigned int *pCount, tABC_Error *pError);
//static void     ABC_TxPrintAddresses(tABC_TxAddress **aAddresses, unsigned int count);
static tABC_CC  ABC_TxAddressAddTx(tABC_TxAddress *pAddress, tABC_Tx *pTx, tABC_Error *pError);
static tABC_CC  ABC_TxTransactionExists(tABC_WalletID self, const char *szID, tABC_Tx **pTx, tABC_Error *pError);
static void     ABC_TxStrTable(const char *needle, int *table);
static int      ABC_TxStrStr(const char *haystack, const char *needle, tABC_Error *pError);
static int      ABC_TxCopyOuputs(tABC_Tx *pTx, tABC_TxOutput **aOutputs, int countOutputs, tABC_Error *pError);
static tABC_CC  ABC_TxWalletOwnsAddress(tABC_WalletID self, const char *szAddress, bool *bFound, tABC_Error *pError);
static tABC_CC  ABC_TxTrashAddresses(tABC_WalletID self, bool bAdd, tABC_Tx *pTx, tABC_TxOutput **paAddresses, unsigned int addressCount, tABC_Error *pError);
static tABC_CC  ABC_TxCalcCurrency(tABC_WalletID self, int64_t amountSatoshi, double *pCurrency, tABC_Error *pError);
static tABC_CC  ABC_TxSendComplete(tABC_WalletID self, tABC_TxSendInfo *pInfo, tABC_UnsavedTx *utx, tABC_Error *pError);

/**
 * Calculates a public address for the HD wallet main external chain.
 * @param pszPubAddress set to the newly-generated address, or set to NULL if
 * there is a math error. If that happens, add 1 to N and try again.
 * @param PrivateSeed any amount of random data to seed the generator
 * @param N the index of the key to generate
 */
static tABC_CC
ABC_BridgeGetBitcoinPubAddress(char **pszPubAddress,
                                       tABC_U08Buf PrivateSeed,
                                       int32_t N,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    libbitcoin::data_chunk seed(PrivateSeed.begin(), PrivateSeed.end());
    libwallet::hd_private_key m(seed);
    libwallet::hd_private_key m0 = m.generate_private_key(0);
    libwallet::hd_private_key m00 = m0.generate_private_key(0);
    libwallet::hd_private_key m00n = m00.generate_private_key(N);
    if (m00n.valid())
    {
        std::string out = m00n.address().encoded();
        ABC_STRDUP(*pszPubAddress, out.c_str());
    }
    else
    {
        *pszPubAddress = nullptr;
    }

exit:
    return cc;
}

/**
 * Allocate a send info struct and populate it with the data given
 */
tABC_CC ABC_TxSendInfoAlloc(tABC_TxSendInfo **ppTxSendInfo,
                            const char *szDestAddress,
                            const tABC_TxDetails *pDetails,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxSendInfo *pTxSendInfo = NULL;

    ABC_CHECK_NULL(ppTxSendInfo);
    ABC_CHECK_NULL(szDestAddress);
    ABC_CHECK_NULL(pDetails);

    ABC_NEW(pTxSendInfo, tABC_TxSendInfo);

    ABC_STRDUP(pTxSendInfo->szDestAddress, szDestAddress);

    ABC_CHECK_RET(ABC_TxDupDetails(&(pTxSendInfo->pDetails), pDetails, pError));

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
        ABC_FREE_STR(pTxSendInfo->szDestAddress);
        ABC_TxFreeDetails(pTxSendInfo->pDetails);
        ABC_WalletIDFree(pTxSendInfo->walletDest);

        ABC_CLEAR_FREE(pTxSendInfo, sizeof(tABC_TxSendInfo));
    }
}

/**
 * Calculates the private keys for a wallet.
 */
static Status
txKeyTableGet(KeyTable &result, tABC_WalletID self)
{
    U08Buf seed; // Do not free
    ABC_CHECK_OLD(ABC_WalletGetBitcoinPrivateSeed(self, &seed, &error));
    libwallet::hd_private_key m(DataChunk(seed.begin(), seed.end()));
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

/**
 * Gets the next unused change address from the wallet.
 */
static Status
txNewChangeAddress(std::string &result, tABC_WalletID self,
    tABC_TxDetails *pDetails)
{
    AutoFree<tABC_TxAddress, ABC_TxFreeAddress> pAddress;
    ABC_CHECK_OLD(ABC_TxCreateNewAddress(self, pDetails, &pAddress.get(), &error));
    ABC_CHECK_OLD(ABC_TxSaveAddress(self, pAddress, &error));

    result = pAddress->szPubAddress;
    return Status();
}

/**
 * Sends the transaction with the given info.
 *
 * @param pInfo Pointer to transaction information
 * @param pszTxID Pointer to hold allocated pointer to transaction ID string
 */
tABC_CC ABC_TxSend(tABC_WalletID self,
                   tABC_TxSendInfo  *pInfo,
                   char             **pszTxID,
                   tABC_Error       *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    std::string changeAddress;
    bc::transaction_type tx;
    AutoFree<tABC_UnsavedTx, ABC_UnsavedTxFree> unsaved;

    ABC_CHECK_NULL(pInfo);

    // Make an unsigned transaction:
    ABC_CHECK_NEW(txNewChangeAddress(changeAddress, self, pInfo->pDetails), pError);
    ABC_CHECK_RET(ABC_BridgeTxMake(self, pInfo, changeAddress, tx, pError));

    // Sign and send transaction:
    {
        Watcher *watcher = nullptr;
        ABC_CHECK_NEW(watcherFind(watcher, self), pError);

        // Sign the transaction:
        KeyTable keys;
        ABC_CHECK_NEW(txKeyTableGet(keys, self), pError);
        ABC_CHECK_NEW(signTx(tx, *watcher, keys), pError);

        // Send to the network:
        bc::data_chunk rawTx(satoshi_raw_size(tx));
        bc::satoshi_save(tx, rawTx.begin());
        ABC_CHECK_NEW(broadcastTx(rawTx), pError);

        // Mark the outputs as spent:
        watcher->send_tx(tx);
        watcherSave(self); // Failure is not fatal
    }

    // Update the ABC db
    ABC_CHECK_RET(ABC_BridgeExtractOutputs(self, &unsaved.get(), tx, pError));
    ABC_CHECK_RET(ABC_TxSendComplete(self, pInfo, unsaved, pError));

    // return the new tx id
    ABC_STRDUP(*pszTxID, unsaved->szTxId);

exit:
    return cc;
}

tABC_CC ABC_TxSendComplete(tABC_WalletID self,
                           tABC_TxSendInfo  *pInfo,
                           tABC_UnsavedTx   *pUtx,
                           tABC_Error       *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    tABC_Tx *pTx = NULL;
    tABC_Tx *pReceiveTx = NULL;
    bool bFound = false;
    double currency;

    // Start watching all addresses incuding new change addres
    ABC_CHECK_RET(ABC_TxWatchAddresses(self, pError));

    // sucessfully sent, now create a transaction
    ABC_NEW(pTx, tABC_Tx);
    ABC_NEW(pTx->pStateInfo, tTxStateInfo);

    // set the state
    pTx->pStateInfo->timeCreation = time(NULL);
    pTx->pStateInfo->bInternal = true;
    ABC_STRDUP(pTx->pStateInfo->szMalleableTxId, pUtx->szTxMalleableId);
    // Copy outputs
    ABC_TxCopyOuputs(pTx, pUtx->aOutputs, pUtx->countOutputs, pError);
    // copy the details
    ABC_CHECK_RET(ABC_TxDupDetails(&(pTx->pDetails), pInfo->pDetails, pError));
    // Add in tx fees to the amount of the tx

    ABC_CHECK_RET(ABC_TxWalletOwnsAddress(self, pInfo->szDestAddress,
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

    ABC_CHECK_RET(ABC_TxCalcCurrency(
        self, pTx->pDetails->amountSatoshi, &currency, pError));
    pTx->pDetails->amountCurrency = currency;

    if (pTx->pDetails->amountSatoshi > 0)
        pTx->pDetails->amountSatoshi *= -1;
    if (pTx->pDetails->amountCurrency > 0)
        pTx->pDetails->amountCurrency *= -1.0;

    // Store transaction ID
    ABC_STRDUP(pTx->szID, pUtx->szTxId);

    // Save the transaction:
    ABC_CHECK_RET(ABC_TxSaveTransaction(self, pTx, pError));

    if (pInfo->bTransfer)
    {
        ABC_NEW(pReceiveTx, tABC_Tx);
        ABC_NEW(pReceiveTx->pStateInfo, tTxStateInfo);

        // set the state
        pReceiveTx->pStateInfo->timeCreation = time(NULL);
        pReceiveTx->pStateInfo->bInternal = true;
        ABC_STRDUP(pReceiveTx->pStateInfo->szMalleableTxId, pUtx->szTxMalleableId);
        // Copy outputs
        ABC_TxCopyOuputs(pReceiveTx, pUtx->aOutputs, pUtx->countOutputs, pError);
        // copy the details
        ABC_CHECK_RET(ABC_TxDupDetails(&(pReceiveTx->pDetails), pInfo->pDetails, pError));

        // Set the payee name:
        AutoFree<tABC_WalletInfo, ABC_WalletFreeInfo> pWallet;
        ABC_CHECK_RET(ABC_WalletGetInfo(self, &pWallet.get(), pError));
        ABC_FREE_STR(pReceiveTx->pDetails->szName);
        ABC_STRDUP(pReceiveTx->pDetails->szName, pWallet->szName);

        pReceiveTx->pDetails->amountSatoshi = pInfo->pDetails->amountSatoshi;

        //
        // Since this wallet is receiving, it didn't really get charged AB fees
        // This should really be an assert since no transfers should have AB fees
        //
        pReceiveTx->pDetails->amountFeesAirbitzSatoshi = 0;

        ABC_CHECK_RET(ABC_TxCalcCurrency(pInfo->walletDest,
            pReceiveTx->pDetails->amountSatoshi, &pReceiveTx->pDetails->amountCurrency, pError));

        if (pReceiveTx->pDetails->amountSatoshi < 0)
            pReceiveTx->pDetails->amountSatoshi *= -1;
        if (pReceiveTx->pDetails->amountCurrency < 0)
            pReceiveTx->pDetails->amountCurrency *= -1.0;

        // Store transaction ID
        ABC_STRDUP(pReceiveTx->szID, pUtx->szTxId);

        // save the transaction
        ABC_CHECK_RET(ABC_TxSaveTransaction(pInfo->walletDest, pReceiveTx, pError));
    }

exit:
    ABC_TxFreeTx(pTx);
    ABC_TxFreeTx(pReceiveTx);
    return cc;
}

tABC_CC  ABC_TxCalcSendFees(tABC_WalletID self, tABC_TxSendInfo *pInfo, uint64_t *pTotalFees, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    std::string changeAddress;
    bc::transaction_type tx;

    ABC_CHECK_NULL(pInfo);

    pInfo->pDetails->amountFeesAirbitzSatoshi = 0;
    pInfo->pDetails->amountFeesMinersSatoshi = 0;

    // Make an unsigned transaction
    ABC_CHECK_NEW(txNewChangeAddress(changeAddress, self, pInfo->pDetails), pError);
    cc = ABC_BridgeTxMake(self, pInfo, changeAddress, tx, pError);

    *pTotalFees = pInfo->pDetails->amountFeesAirbitzSatoshi
                + pInfo->pDetails->amountFeesMinersSatoshi;
    ABC_CHECK_RET(cc);

exit:
    return cc;
}

tABC_CC ABC_TxWalletOwnsAddress(tABC_WalletID self,
                                const char *szAddress,
                                bool *bFound,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoStringArray addresses;

    ABC_CHECK_RET(
        ABC_TxGetPubAddresses(self, &addresses.data, &addresses.size, pError));
    *bFound = false;
    for (unsigned i = 0; i < addresses.size; ++i)
    {
        if (strncmp(szAddress, addresses.data[i], strlen(szAddress)) == 0)
        {
            *bFound = true;
            break;
        }
    }
exit:
    return cc;
}

/**
 * Gets the public addresses associated with the given wallet.
 *
 * @param paAddresses       Pointer to string array of addresses
 * @param pCount            Pointer to store number of addresses
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetPubAddresses(tABC_WalletID self,
                              char ***paAddresses,
                              unsigned int *pCount,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_TxAddress **aAddresses = NULL;
    char **sAddresses;
    unsigned int countAddresses = 0;
    ABC_CHECK_RET(
        ABC_TxGetAddresses(self, &aAddresses, &countAddresses, pError));
    ABC_ARRAY_NEW(sAddresses, countAddresses, char*);
    for (unsigned i = 0; i < countAddresses; i++)
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

tABC_CC ABC_TxWatchAddresses(tABC_WalletID self,
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
 * Duplicate a TX details struct
 */
tABC_CC ABC_TxDupDetails(tABC_TxDetails **ppNewDetails, const tABC_TxDetails *pOldDetails, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxDetails *pNewDetails = NULL;

    ABC_CHECK_NULL(ppNewDetails);
    ABC_CHECK_NULL(pOldDetails);

    ABC_NEW(pNewDetails, tABC_TxDetails);

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

tABC_CC
ABC_TxBlockHeightUpdate(uint64_t height,
                        tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                        void *pData,
                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    if (fAsyncBitCoinEventCallback)
    {
        tABC_AsyncBitCoinInfo info;
        info.eventType = ABC_AsyncEventType_BlockHeightChange;
        info.pData = pData;
        ABC_STRDUP(info.szDescription, "Block height change");
        fAsyncBitCoinEventCallback(&info);
        ABC_FREE_STR(info.szDescription);
    }
exit:
    return cc;
}

/**
 * Handles creating or updating when we receive a transaction
 */
tABC_CC ABC_TxReceiveTransaction(tABC_WalletID self,
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
        ABC_NEW(pTx, tABC_Tx);
        ABC_NEW(pTx->pStateInfo, tTxStateInfo);
        ABC_NEW(pTx->pDetails, tABC_TxDetails);

        ABC_STRDUP(pTx->pStateInfo->szMalleableTxId, szMalTxId);
        pTx->pStateInfo->timeCreation = time(NULL);
        pTx->pDetails->amountSatoshi = amountSatoshi;
        pTx->pDetails->amountCurrency = currency;
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
        ABC_ARRAY_NEW(pTx->aOutputs, pTx->countOutputs, tABC_TxOutput*);
        for (unsigned i = 0; i < inAddressCount; ++i)
        {
            ABC_DebugLog("Saving Input address: %s\n", paInAddresses[i]->szAddress);

            ABC_NEW(pTx->aOutputs[i], tABC_TxOutput);
            ABC_STRDUP(pTx->aOutputs[i]->szAddress, paInAddresses[i]->szAddress);
            ABC_STRDUP(pTx->aOutputs[i]->szTxId, paInAddresses[i]->szTxId);
            pTx->aOutputs[i]->input = paInAddresses[i]->input;
            pTx->aOutputs[i]->value = paInAddresses[i]->value;
        }
        for (unsigned i = 0; i < outAddressCount; ++i)
        {
            ABC_DebugLog("Saving Output address: %s\n", paOutAddresses[i]->szAddress);
            int newi = i + inAddressCount;
            ABC_NEW(pTx->aOutputs[newi], tABC_TxOutput);
            ABC_STRDUP(pTx->aOutputs[newi]->szAddress, paOutAddresses[i]->szAddress);
            ABC_STRDUP(pTx->aOutputs[newi]->szTxId, paOutAddresses[i]->szTxId);
            pTx->aOutputs[newi]->input = paOutAddresses[i]->input;
            pTx->aOutputs[newi]->value = paOutAddresses[i]->value;
        }

        // save the transaction
        ABC_CHECK_RET(
            ABC_TxSaveTransaction(self, pTx, pError));

        // add the transaction to the address
        ABC_CHECK_RET(ABC_TxTrashAddresses(self, true,
                        pTx, paOutAddresses, outAddressCount, pError));

        // Mark the wallet cache as dirty in case the Tx wasn't included in the current balance
        ABC_CHECK_RET(ABC_WalletDirtyCache(self, pError));

        if (fAsyncBitCoinEventCallback)
        {
            tABC_AsyncBitCoinInfo info;
            info.pData = pData;
            info.eventType = ABC_AsyncEventType_IncomingBitCoin;
            ABC_STRDUP(info.szTxID, pTx->szID);
            ABC_STRDUP(info.szWalletUUID, self.szUUID);
            ABC_STRDUP(info.szDescription, "Received funds");
            fAsyncBitCoinEventCallback(&info);
            ABC_FREE_STR(info.szTxID);
            ABC_FREE_STR(info.szDescription);
        }
    }
    else
    {
        ABC_DebugLog("We already have %s\n", szTxId);
        ABC_CHECK_RET(ABC_TxTrashAddresses(self, false,
                        pTx, paOutAddresses, outAddressCount, pError));

        // Mark the wallet cache as dirty in case the Tx wasn't included in the current balance
        ABC_CHECK_RET(ABC_WalletDirtyCache(self, pError));

        if (fAsyncBitCoinEventCallback)
        {
            tABC_AsyncBitCoinInfo info;
            info.pData = pData;
            info.eventType = ABC_AsyncEventType_DataSyncUpdate;
            ABC_STRDUP(info.szTxID, pTx->szID);
            ABC_STRDUP(info.szWalletUUID, self.szUUID);
            ABC_STRDUP(info.szDescription, "Updated balance");
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
 * Marks the address as unusable and copies the details to the new Tx
 *
 * @param pTx           The transaction that will be updated
 * @param paAddress     Addresses that will be search
 * @param addressCount  Number of address in paAddress
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxTrashAddresses(tABC_WalletID self,
                             bool bAdd,
                             tABC_Tx *pTx,
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

    for (unsigned i = 0; i < addressCount; ++i)
    {
        std::string addr(paAddresses[i]->szAddress);
        if (addrMap.find(addr) == addrMap.end())
            continue;

        pAddress = addrMap[addr];
        if (pAddress)
        {
            pAddress->pStateInfo->bRecycleable = false;
            if (bAdd)
            {
                ABC_CHECK_RET(ABC_TxAddressAddTx(pAddress, pTx, pError));
            }
            ABC_CHECK_RET(ABC_TxSaveAddress(self,
                    pAddress, pError));
            int changed = 0;
            if (ABC_STRLEN(pTx->pDetails->szName) == 0
                    && ABC_STRLEN(pAddress->pDetails->szName) > 0)
            {
                ABC_STRDUP(pTx->pDetails->szName, pAddress->pDetails->szName);
                ++changed;
            }
            if (ABC_STRLEN(pTx->pDetails->szNotes) == 0
                    && ABC_STRLEN(pAddress->pDetails->szNotes) > 0)
            {
                ABC_STRDUP(pTx->pDetails->szNotes, pAddress->pDetails->szNotes);
                ++changed;
            }
            if (ABC_STRLEN(pTx->pDetails->szCategory) == 0
                    && ABC_STRLEN(pAddress->pDetails->szCategory))
            {
                ABC_STRDUP(pTx->pDetails->szCategory, pAddress->pDetails->szCategory);
                ++changed;
            }
            if (changed)
            {
                ABC_CHECK_RET(
                    ABC_TxSaveTransaction(self, pTx, pError));
            }
        }
        pAddress = NULL;
    }

exit:
    ABC_TxFreeAddresses(pInternalAddress, localCount);

    return cc;
}

/**
 * Calculates the amount of currency based off of Wallet's currency code
 *
 * @param tABC_WalletID
 * @param amountSatoshi
 * @param pCurrency     Point to double
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxCalcCurrency(tABC_WalletID self, int64_t amountSatoshi,
                           double *pCurrency, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    double currency = 0.0;
    tABC_WalletInfo *pWallet = NULL;

    ABC_CHECK_RET(ABC_WalletGetInfo(self, &pWallet, pError));
    ABC_CHECK_NEW(exchangeSatoshiToCurrency(
        currency, amountSatoshi, static_cast<Currency>(pWallet->currencyNum)), pError);

    *pCurrency = currency;
exit:
    ABC_WalletFreeInfo(pWallet);

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
tABC_CC ABC_TxCreateReceiveRequest(tABC_WalletID self,
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
    ABC_STRDUP(*pszRequestID, pAddress->szID);

    // Watch this new address
    ABC_CHECK_RET(ABC_TxWatchAddresses(self, pError));

exit:
    ABC_TxFreeAddress(pAddress);

    return cc;
}

tABC_CC ABC_TxCreateInitialAddresses(tABC_WalletID self,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxDetails *pDetails = NULL;
    ABC_NEW(pDetails, tABC_TxDetails);
    ABC_STRDUP(pDetails->szName, "");
    ABC_STRDUP(pDetails->szCategory, "");
    ABC_STRDUP(pDetails->szNotes, "");
    pDetails->attributes = 0x0;
    pDetails->bizId = 0;

    ABC_CHECK_RET(ABC_TxCreateNewAddress(self, pDetails, NULL, pError));
exit:
    ABC_FreeTxDetails(pDetails);

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
tABC_CC ABC_TxCreateNewAddress(tABC_WalletID self,
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
        if (aAddresses[i]->pStateInfo->bRecycleable == true
                && aAddresses[i]->pStateInfo->countActivities == 0)
        {
            recyclable++;
            if (pAddress == NULL)
            {
                char *szRegenAddress = NULL;
                AutoU08Buf Seed;
                ABC_CHECK_RET(ABC_WalletGetBitcoinPrivateSeedDisk(self, &Seed, pError));
                ABC_CHECK_RET(ABC_BridgeGetBitcoinPubAddress(&szRegenAddress, Seed, aAddresses[i]->seq, pError));

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
        ABC_FreeTxDetails(pAddress->pDetails);
        pAddress->pDetails = NULL;

        // copy over the info we were given
        ABC_CHECK_RET(ABC_DuplicateTxDetails(&(pAddress->pDetails), pDetails, pError));

        // create the state info
        ABC_NEW(pAddress->pStateInfo, tTxAddressStateInfo);
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
tABC_CC ABC_TxCreateNewAddressForN(tABC_WalletID self, int32_t N, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    tABC_TxAddress *pAddress = NULL;
    AutoU08Buf Seed;

    // Now we know the latest N, create a new address
    ABC_NEW(pAddress, tABC_TxAddress);

    // get the private seed so we can generate the public address
    ABC_CHECK_RET(ABC_WalletGetBitcoinPrivateSeedDisk(self, &Seed, pError));

    // generate the public address
    pAddress->szPubAddress = NULL;
    pAddress->seq = N;
    do
    {
        // move to the next sequence number
        pAddress->seq++;

        // Get the public address for our sequence (it can return NULL, if it is invalid)
        ABC_CHECK_RET(ABC_BridgeGetBitcoinPubAddress(&(pAddress->szPubAddress), Seed, pAddress->seq, pError));
    } while (pAddress->szPubAddress == NULL);

    // set the final ID
    ABC_STR_NEW(pAddress->szID, TX_MAX_ADDR_ID_LENGTH);
    sprintf(pAddress->szID, "%u", pAddress->seq);

    ABC_NEW(pAddress->pStateInfo, tTxAddressStateInfo);
    pAddress->pStateInfo->bRecycleable = true;
    pAddress->pStateInfo->countActivities = 0;
    pAddress->pStateInfo->aActivities = NULL;
    pAddress->pStateInfo->timeCreation = time(NULL);

    ABC_NEW(pAddress->pDetails, tABC_TxDetails);
    ABC_STRDUP(pAddress->pDetails->szName, "");
    ABC_STRDUP(pAddress->pDetails->szCategory, "");
    ABC_STRDUP(pAddress->pDetails->szNotes, "");
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
tABC_CC ABC_TxModifyReceiveRequest(tABC_WalletID self,
                                   const char *szRequestID,
                                   tABC_TxDetails *pDetails,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFile = NULL;
    std::string path;
    tABC_TxAddress *pAddress = NULL;
    tABC_TxDetails *pNewDetails = NULL;

    // get the filename for this request (note: interally requests are addresses)
    ABC_CHECK_RET(ABC_GetAddressFilename(self.szUUID, szRequestID, &szFile, pError));
    path = walletAddressDir(self.szUUID) + szFile;

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(self, path.c_str(), &pAddress, pError));

    // copy the new details
    ABC_CHECK_RET(ABC_TxDupDetails(&pNewDetails, pDetails, pError));

    // free the old details on this address
    ABC_TxFreeDetails(pAddress->pDetails);

    // set the new details
    pAddress->pDetails = pNewDetails;
    pNewDetails = NULL;

    // write out the address
    ABC_CHECK_RET(ABC_TxSaveAddress(self, pAddress, pError));

exit:
    ABC_FREE_STR(szFile);
    ABC_TxFreeAddress(pAddress);
    ABC_TxFreeDetails(pNewDetails);

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
    AutoFileLock lock(gFileMutex); // We are iterating over the filesystem

    std::string addressDir = walletAddressDir(szWalletUUID);
    tABC_FileIOList *pFileList = NULL;
    bool bExists = false;
    char *szID = NULL;

    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szAddressID);
    ABC_CHECK_ASSERT(strlen(szAddressID) > 0, ABC_CC_Error, "No address UUID provided");
    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;

    // Make sure there is an addresses directory
    ABC_CHECK_RET(ABC_FileIOFileExists(addressDir.c_str(), &bExists, pError));
    ABC_CHECK_ASSERT(bExists == true, ABC_CC_Error, "No existing requests/addresses");

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
                ABC_STRDUP(*pszFilename, pFileList->apFiles[i]->szName);
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
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxFinalizeReceiveRequest(tABC_WalletID self,
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
tABC_CC ABC_TxCancelReceiveRequest(tABC_WalletID self,
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
tABC_CC ABC_TxSetAddressRecycle(tABC_WalletID self,
                                const char *szAddress,
                                bool bRecyclable,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    std::string path;
    char *szFile = NULL;
    tABC_TxAddress *pAddress = NULL;
    tABC_TxDetails *pNewDetails = NULL;

    // get the filename for this address
    ABC_CHECK_RET(ABC_GetAddressFilename(self.szUUID, szAddress, &szFile, pError));
    path = walletAddressDir(self.szUUID) + szFile;

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(self, path.c_str(), &pAddress, pError));
    ABC_CHECK_NULL(pAddress->pStateInfo);

    // if it isn't already set as required
    if (pAddress->pStateInfo->bRecycleable != bRecyclable)
    {
        // change the recycle boolean
        ABC_CHECK_NULL(pAddress->pStateInfo);
        pAddress->pStateInfo->bRecycleable = bRecyclable;

        // write out the address
        ABC_CHECK_RET(ABC_TxSaveAddress(self, pAddress, pError));
    }

exit:
    ABC_FREE_STR(szFile);
    ABC_TxFreeAddress(pAddress);
    ABC_TxFreeDetails(pNewDetails);

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
tABC_CC ABC_TxGenerateRequestQRCode(tABC_WalletID self,
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
        ABC_STRDUP(*pszURI, szURI);
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
tABC_CC ABC_TxGetTransaction(tABC_WalletID self,
                             const char *szID,
                             tABC_TxInfo **ppTransaction,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;
    tABC_TxInfo *pTransaction = NULL;
    bool bExists = false;

    *ppTransaction = NULL;

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, true, pError));
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));

    // if the internal doesn't exist
    if (bExists == false)
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, false, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    }

    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

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
tABC_CC ABC_TxGetTransactions(tABC_WalletID self,
                              int64_t startTime,
                              int64_t endTime,
                              tABC_TxInfo ***paTransactions,
                              unsigned int *pCount,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    AutoFileLock fileLock(gFileMutex); // We are iterating over the filesystem

    std::string txDir = walletTxDir(self.szUUID);
    tABC_FileIOList *pFileList = NULL;
    tABC_TxInfo **aTransactions = NULL;
    unsigned int count = 0;
    bool bExists = false;

    *paTransactions = NULL;
    *pCount = 0;

    // if there is a transaction directory
    ABC_CHECK_RET(ABC_FileIOFileExists(txDir.c_str(), &bExists, pError));

    if (bExists == true)
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
                    if (bHasInternalEquivalent == false)
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
tABC_CC ABC_TxSearchTransactions(tABC_WalletID self,
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

        // check if this internal version of the file exists
        bool bExists = false;
        ABC_CHECK_RET(ABC_FileIOFileExists(name.c_str(), &bExists, pError));

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
 * @param szFilename        Filename of the transaction
 * @param ppTransaction     Location to store allocated transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadTransactionInfo(tABC_WalletID self,
                                  const char *szFilename,
                                  tABC_TxInfo **ppTransaction,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    tABC_Tx *pTx = NULL;
    tABC_TxInfo *pTransaction = NULL;

    *ppTransaction = NULL;

    // load the transaction
    ABC_CHECK_RET(ABC_TxLoadTransaction(self, szFilename, &pTx, pError));
    ABC_CHECK_NULL(pTx->pDetails);
    ABC_CHECK_NULL(pTx->pStateInfo);

    // steal the data and assign it to our new struct
    ABC_NEW(pTransaction, tABC_TxInfo);
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
tABC_CC ABC_TxLoadTxAndAppendToArray(tABC_WalletID self,
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
tABC_CC ABC_TxSetTransactionDetails(tABC_WalletID self,
                                    const char *szID,
                                    tABC_TxDetails *pDetails,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;
    bool bExists = false;

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, true, pError));
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));

    // if the internal doesn't exist
    if (bExists == false)
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, false, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    }

    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

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
    ABC_STRDUP(pTx->pDetails->szName, pDetails->szName);
    ABC_FREE_STR(pTx->pDetails->szCategory);
    ABC_STRDUP(pTx->pDetails->szCategory, pDetails->szCategory);
    ABC_FREE_STR(pTx->pDetails->szNotes);
    ABC_STRDUP(pTx->pDetails->szNotes, pDetails->szNotes);

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
tABC_CC ABC_TxGetTransactionDetails(tABC_WalletID self,
                                    const char *szID,
                                    tABC_TxDetails **ppDetails,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;
    tABC_TxDetails *pDetails = NULL;
    bool bExists = false;

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, true, pError));
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));

    // if the internal doesn't exist
    if (bExists == false)
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, false, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    }

    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

    // load the existing transaction
    ABC_CHECK_RET(ABC_TxLoadTransaction(self, szFilename, &pTx, pError));
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

    return cc;
}

/**
 * Gets the bit coin public address for a specified request
 *
 * @param szRequestID       ID of request
 * @param pszAddress        Location to store allocated address string (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetRequestAddress(tABC_WalletID self,
                                const char *szRequestID,
                                char **pszAddress,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    tABC_TxAddress *pAddress = NULL;

    *pszAddress = NULL;

    ABC_CHECK_RET(
        ABC_TxLoadAddress(self, szRequestID,
                          &pAddress, pError));
    ABC_STRDUP(*pszAddress, pAddress->szPubAddress);
exit:
    ABC_TxFreeAddress(pAddress);

    return cc;
}

/**
 * Gets the pending requests associated with the given wallet.
 *
 * @param paTransactions    Pointer to store array of requests info pointers
 * @param pCount            Pointer to store number of requests
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetPendingRequests(tABC_WalletID self,
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
        // print out the addresses - debug
        //ABC_TxPrintAddresses(aAddresses, count);

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
                            ABC_NEW(pRequest, tABC_RequestInfo);
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
tABC_CC ABC_TxBuildFromLabel(tABC_WalletID self,
                             char **pszLabel, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoFree<tABC_AccountSettings, ABC_FreeAccountSettings> pSettings;

    ABC_CHECK_NULL(pszLabel);
    *pszLabel = NULL;

    ABC_CHECK_RET(ABC_AccountSettingsLoad(*self.account, &pSettings.get(), pError));

    if (pSettings->bNameOnPayments && pSettings->szFullName)
    {
        ABC_STRDUP(*pszLabel, pSettings->szFullName);
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
tABC_CC ABC_TxSweepSaveTransaction(tABC_WalletID wallet,
                                   const char *txId,
                                   const char *malTxId,
                                   uint64_t funds,
                                   tABC_TxDetails *pDetails,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Tx *pTx = NULL;
    tABC_WalletInfo *pWalletInfo = NULL;
    double currency;

    ABC_NEW(pTx, tABC_Tx);
    ABC_NEW(pTx->pStateInfo, tTxStateInfo);

    // set the state
    pTx->pStateInfo->timeCreation = time(NULL);
    pTx->pStateInfo->bInternal = true;
    ABC_STRDUP(pTx->szID, txId);
    ABC_STRDUP(pTx->pStateInfo->szMalleableTxId, malTxId);

    // Copy the details
    ABC_CHECK_RET(ABC_TxDupDetails(&(pTx->pDetails), pDetails, pError));
    pTx->pDetails->amountSatoshi = funds;
    pTx->pDetails->amountFeesAirbitzSatoshi = 0;

    ABC_CHECK_RET(ABC_WalletGetInfo(wallet, &pWalletInfo, pError));
    ABC_CHECK_NEW(exchangeSatoshiToCurrency(
        currency, pTx->pDetails->amountSatoshi,
        static_cast<Currency>(pWalletInfo->currencyNum)), pError);
    pTx->pDetails->amountCurrency = currency;

    // save the transaction
    ABC_CHECK_RET(ABC_TxSaveTransaction(wallet, pTx, pError));
exit:
    ABC_TxFreeTx(pTx);
    ABC_WalletFreeInfo(pWalletInfo);
    return cc;
}


/**
 * Gets the filename for a given transaction
 * format is: N-Base58(HMAC256(TxID,MK)).json
 *
 * @param pszFilename Output filename name. The caller must free this.
 */
static
tABC_CC ABC_TxCreateTxFilename(tABC_WalletID self, char **pszFilename, const char *szTxID, bool bInternal, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    U08Buf MK; // Do not free
    std::string path;

    *pszFilename = NULL;

    // Get the master key we will need to encode the filename
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(self, &MK, pError));

    path = walletTxDir(self.szUUID) + cryptoFilename(MK, szTxID) +
        (bInternal ? TX_INTERNAL_SUFFIX : TX_EXTERNAL_SUFFIX);
    ABC_STRDUP(*pszFilename, path.c_str());

exit:
    return cc;
}

/**
 * Loads a transaction from disk
 *
 * @param ppTx  Pointer to location to hold allocated transaction
 *              (it is the callers responsiblity to free this transaction)
 */
static
tABC_CC ABC_TxLoadTransaction(tABC_WalletID self,
                              const char *szFilename,
                              tABC_Tx **ppTx,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    U08Buf MK; // Do not free
    json_t *pJSON_Root = NULL;
    tABC_Tx *pTx = NULL;
    bool bExists = false;
    json_t *jsonVal = NULL;

    *ppTx = NULL;

    // Get the master key we will need to decode the transaction data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(self, &MK, pError));

    // make sure the transaction exists
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

    // load the json object (load file, decrypt it, create json object
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, MK, &pJSON_Root, pError));

    // start decoding

    ABC_NEW(pTx, tABC_Tx);

    // get the id
    jsonVal = json_object_get(pJSON_Root, JSON_TX_ID_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing id");
    ABC_STRDUP(pTx->szID, json_string_value(jsonVal));

    // get the state object
    ABC_CHECK_RET(ABC_TxDecodeTxState(pJSON_Root, &(pTx->pStateInfo), pError));

    // get the details object
    ABC_CHECK_RET(ABC_TxDecodeTxDetails(pJSON_Root, &(pTx->pDetails), pError));

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

    tTxStateInfo *pInfo = NULL;
    json_t *jsonState = NULL;
    json_t *jsonVal = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppInfo);
    *ppInfo = NULL;

    // allocate the struct
    ABC_NEW(pInfo, tTxStateInfo);

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
    json_t *jsonDetails = NULL;
    json_t *jsonVal = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppDetails);
    *ppDetails = NULL;

    // allocated the struct
    ABC_NEW(pDetails, tABC_TxDetails);

    // get the details object
    jsonDetails = json_object_get(pJSON_Obj, JSON_DETAILS_FIELD);
    ABC_CHECK_ASSERT((jsonDetails && json_is_object(jsonDetails)), ABC_CC_JSONError, "Error parsing JSON details package - missing meta data (details)");

    // get the satoshi field
    jsonVal = json_object_get(jsonDetails, JSON_AMOUNT_SATOSHI_FIELD);
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
 * Saves a transaction to disk
 *
 * @param pTx  Pointer to transaction data
 */
static
tABC_CC ABC_TxSaveTransaction(tABC_WalletID self,
                              const tABC_Tx *pTx,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    int e;

    U08Buf MK; // Do not free
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;
    json_t *pJSON_OutputArray = NULL;
    json_t **ppJSON_Output = NULL;

    ABC_CHECK_NULL(pTx->pStateInfo);
    ABC_CHECK_NULL(pTx->szID);
    ABC_CHECK_ASSERT(strlen(pTx->szID) > 0, ABC_CC_Error, "No transaction ID provided");

    // Get the master key we will need to encode the transaction data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(self, &MK, pError));

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
    ABC_CHECK_NEW(fileEnsureDir(walletTxDir(self.szUUID)), pError);

    // get the filename for this transaction
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, pTx->szID, pTx->pStateInfo->bInternal, pError));

    // save out the transaction object to a file encrypted with the master key
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, MK, ABC_CryptoType_AES256, szFilename, pError));

    ABC_CHECK_RET(ABC_WalletDirtyCache(self, pError));
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
 * @param szAddressID   ID of the address
 * @param ppAddress     Pointer to location to store allocated address info
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadAddress(tABC_WalletID self,
                          const char *szAddressID,
                          tABC_TxAddress **ppAddress,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *szFile = NULL;
    std::string path;

    // get the filename for this address
    ABC_CHECK_RET(ABC_GetAddressFilename(self.szUUID, szAddressID, &szFile, pError));
    path = walletAddressDir(self.szUUID) + szFile;

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
tABC_CC ABC_TxLoadAddressFile(tABC_WalletID self,
                              const char *szFilename,
                              tABC_TxAddress **ppAddress,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    U08Buf MK; // Do not free
    json_t *pJSON_Root = NULL;
    tABC_TxAddress *pAddress = NULL;
    bool bExists = false;
    json_t *jsonVal = NULL;

    *ppAddress = NULL;

    // Get the master key we will need to decode the transaction data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(self, &MK, pError));

    // make sure the addresss exists
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoRequest, "Request address does not exist");

    // load the json object (load file, decrypt it, create json object
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, MK, &pJSON_Root, pError));

    // start decoding

    ABC_NEW(pAddress, tABC_TxAddress);

    // get the seq and id
    jsonVal = json_object_get(pJSON_Root, JSON_ADDR_SEQ_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing seq");
    pAddress->seq = (uint32_t)json_integer_value(jsonVal);
    ABC_STR_NEW(pAddress->szID, TX_MAX_ADDR_ID_LENGTH);
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
    json_t *jsonState = NULL;
    json_t *jsonVal = NULL;
    json_t *jsonActivity = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppState);
    *ppState = NULL;

    // allocated the struct
    ABC_NEW(pState, tTxAddressStateInfo);

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
tABC_CC ABC_TxSaveAddress(tABC_WalletID self,
                          const tABC_TxAddress *pAddress,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    U08Buf MK; // Do not free
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL(pAddress->pStateInfo);
    ABC_CHECK_NULL(pAddress->szID);
    ABC_CHECK_ASSERT(strlen(pAddress->szID) > 0, ABC_CC_Error, "No address ID provided");

    // Get the master key we will need to encode the address data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(self, &MK, pError));

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
    ABC_CHECK_NEW(fileEnsureDir(walletAddressDir(self.szUUID)), pError);

    // create the filename for this transaction
    ABC_CHECK_RET(ABC_TxCreateAddressFilename(self, &szFilename, pAddress, pError));

    // save out the transaction object to a file encrypted with the master key
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, MK, ABC_CryptoType_AES256, szFilename, pError));

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
tABC_CC ABC_TxCreateAddressFilename(tABC_WalletID self, char **pszFilename, const tABC_TxAddress *pAddress, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    U08Buf MK; // Do not free
    std::string path;

    *pszFilename = NULL;

    // Get the master key we will need to encode the filename
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(self, &MK, pError));

    path = walletAddressDir(self.szUUID) +
        std::to_string(pAddress->seq) + "-" +
        cryptoFilename(MK, pAddress->szPubAddress) + ".json";
    ABC_STRDUP(*pszFilename, path.c_str());

exit:
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
tABC_CC ABC_TxGetAddresses(tABC_WalletID self,
                           tABC_TxAddress ***paAddresses,
                           unsigned int *pCount,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    AutoFileLock fileLock(gFileMutex); // We are iterating over the filesystem

    std::string addressDir = walletAddressDir(self.szUUID);
    tABC_FileIOList *pFileList = NULL;
    tABC_TxAddress **aAddresses = NULL;
    unsigned int count = 0;
    bool bExists = false;

    *paAddresses = NULL;
    *pCount = 0;

    // if there is a address directory
    ABC_CHECK_RET(ABC_FileIOFileExists(addressDir.c_str(), &bExists, pError));

    if (bExists == true)
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
tABC_CC ABC_TxLoadAddressAndAppendToArray(tABC_WalletID self,
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
    ABC_ARRAY_RESIZE(aActivities, countActivities + 1, tTxAddressActivity);

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

tABC_CC ABC_TxTransactionExists(tABC_WalletID self,
                                const char *szID,
                                tABC_Tx **pTx,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    char *szFilename = NULL;
    bool bExists = false;

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, true, pError));
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));

    // if the internal doesn't exist
    if (bExists == false)
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(self, &szFilename, szID, false, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    }
    if (bExists)
    {
        ABC_CHECK_RET(ABC_TxLoadTransaction(self, szFilename, pTx, pError));
    } else {
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
            ABC_NEW(pTx->aOutputs[i], tABC_TxOutput);
            ABC_STRDUP(pTx->aOutputs[i]->szAddress, aOutputs[i]->szAddress);
            ABC_STRDUP(pTx->aOutputs[i]->szTxId, aOutputs[i]->szTxId);
            pTx->aOutputs[i]->input = aOutputs[i]->input;
            pTx->aOutputs[i]->value = aOutputs[i]->value;
        }
    }
exit:
    return cc;
}

} // namespace abcd
