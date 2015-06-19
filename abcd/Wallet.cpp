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

#include "Wallet.hpp"
#include "Context.hpp"
#include "Tx.hpp"
#include "account/Account.hpp"
#include "bitcoin/WatcherBridge.hpp"
#include "crypto/Crypto.hpp"
#include "crypto/Encoding.hpp"
#include "crypto/Random.hpp"
#include "login/Lobby.hpp"
#include "login/Login.hpp"
#include "login/LoginServer.hpp"
#include "util/FileIO.hpp"
#include "util/Json.hpp"
#include "util/Mutex.hpp"
#include "util/Sync.hpp"
#include "util/Util.hpp"
#include "wallet/Wallet.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

namespace abcd {

#define WALLET_NAME_FILENAME                    "WalletName.json"
#define WALLET_CURRENCY_FILENAME                "Currency.json"

#define JSON_WALLET_NAME_FIELD                  "walletName"
#define JSON_WALLET_CURRENCY_NUM_FIELD          "num"

struct WalletJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(WalletJson, JsonObject)
    ABC_JSON_STRING(dataKey,    "MK",           nullptr)
    ABC_JSON_STRING(syncKey,    "SyncKey",      nullptr)
    ABC_JSON_STRING(bitcoinKey, "BitcoinSeed",  nullptr)
    // There are other fields, but the WalletList handles those.
};

// holds wallet data (including keys) for a given wallet
typedef struct sWalletData
{
    char            *szUUID;
    char            *szName;
    char            *szWalletAcctKey;
    int             currencyNum;
    tABC_U08Buf     MK;
    tABC_U08Buf     BitcoinPrivateSeed;
    bool            balanceDirty;
    int64_t         balance;
} tWalletData;

// this holds all the of the currently cached wallets
static unsigned int gWalletsCacheCount = 0;
static tWalletData **gaWalletsCacheArray = NULL;

static tABC_CC ABC_WalletSetCurrencyNum(Wallet &self, int currencyNum, tABC_Error *pError);
static tABC_CC ABC_WalletCacheData(Wallet &self, tWalletData **ppData, tABC_Error *pError);
static tABC_CC ABC_WalletAddToCache(tWalletData *pData, tABC_Error *pError);
static tABC_CC ABC_WalletRemoveFromCache(const char *szUUID, tABC_Error *pError);
static tABC_CC ABC_WalletGetFromCacheByUUID(const char *szUUID, tWalletData **ppData, tABC_Error *pError);
static void    ABC_WalletFreeData(tWalletData *pData);

/**
 * Creates the wallet with the given info.
 *
 * @param pszUUID Pointer to hold allocated pointer to UUID string
 */
tABC_CC ABC_WalletCreate(Account &account,
                         const char *szWalletName,
                         int  currencyNum,
                         char                  **pszUUID,
                         tABC_Error            *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Data     = NULL;
    json_t *pJSON_Wallets  = NULL;
    std::string uuid;
    std::string dir;
    std::string syncDir;
    DataChunk dataKey;
    DataChunk syncKey;
    DataChunk bitcoinKey;
    bool dirty = false;
    AutoU08Buf LP1;
    WalletJson json;
    std::shared_ptr<Wallet> wallet;

    tWalletData *pData = NULL;

    ABC_CHECK_NULL(pszUUID);
    ABC_CHECK_RET(ABC_LoginGetServerKey(account.login, &LP1, pError));

    // create a new wallet data struct
    ABC_NEW(pData, tWalletData);

    // create wallet guid
    ABC_CHECK_NEW(randomUuid(uuid));
    ABC_STRDUP(pData->szUUID, uuid.c_str());
    ABC_STRDUP(*pszUUID, uuid.c_str());
    ABC_CHECK_NEW(Wallet::create(wallet, account, uuid));

    // generate the master key for this wallet - MK_<Wallet_GUID1>
    ABC_CHECK_NEW(randomData(dataKey, DATA_KEY_LENGTH));
    ABC_BUF_DUP(pData->MK, toU08Buf(dataKey));

    // create and set the bitcoin private seed for this wallet
    ABC_CHECK_NEW(randomData(bitcoinKey, BITCOIN_SEED_LENGTH));
    ABC_BUF_DUP(pData->BitcoinPrivateSeed, toU08Buf(bitcoinKey));

    // Create Wallet Repo key
    ABC_CHECK_NEW(randomData(syncKey, SYNC_KEY_LENGTH));
    ABC_STRDUP(pData->szWalletAcctKey, base16Encode(syncKey).c_str());

    // create the wallet root directory if necessary
    ABC_CHECK_NEW(fileEnsureDir(gContext->walletsDir()));

    // create the wallet directory - <Wallet_UUID1>  <- All data in this directory encrypted with MK_<Wallet_UUID1>
    dir = wallet->dir();
    ABC_CHECK_NEW(fileEnsureDir(dir));

    // create the wallet sync dir under the main dir
    syncDir = wallet->syncDir();
    ABC_CHECK_NEW(fileEnsureDir(syncDir));
    ABC_CHECK_NEW(syncMakeRepo(syncDir));

    // we now have a new wallet so go ahead and cache its data
    ABC_CHECK_RET(ABC_WalletAddToCache(pData, pError));

    // all the functions below assume the wallet is in the cache or can be loaded into the cache
    // set the wallet name
    ABC_CHECK_RET(ABC_WalletSetName(*wallet, szWalletName, pError));

    // set the currency
    ABC_CHECK_RET(ABC_WalletSetCurrencyNum(*wallet, currencyNum, pError));

    // Request remote wallet repo
    ABC_CHECK_NEW(LoginServerWalletCreate(account.login.lobby, LP1, pData->szWalletAcctKey));

    // TODO: should probably add the creation date to optimize wallet export (assuming it is even used)

    // Upload the initial files:
    ABC_CHECK_NEW(syncRepo(syncDir, pData->szWalletAcctKey, dirty));

    // Actiate the remote wallet
    ABC_CHECK_NEW(LoginServerWalletActivate(account.login.lobby, LP1, pData->szWalletAcctKey));

    // If everything worked, add the wallet to the account:
    ABC_CHECK_NEW(json.dataKeySet(base16Encode(dataKey)));
    ABC_CHECK_NEW(json.syncKeySet(base16Encode(syncKey)));
    ABC_CHECK_NEW(json.bitcoinKeySet(base16Encode(bitcoinKey)));
    ABC_CHECK_NEW(account.wallets.insert(uuid, json));

    // Now the wallet is written to disk, generate some addresses
    ABC_CHECK_RET(ABC_TxCreateInitialAddresses(*wallet, pError));

    // After wallet is created, sync the account:
    ABC_CHECK_NEW(account.sync(dirty));

    pData = NULL; // so we don't free what we just added to the cache
exit:
    if (cc != ABC_CC_Ok)
    {
        if (!uuid.empty())
        {
            ABC_WalletRemoveFromCache(uuid.c_str(), NULL);
        }
        if (!dir.empty())
        {
            ABC_FileIODeleteRecursive(dir.c_str(), NULL);
        }
    }
    if (pJSON_Data)         json_decref(pJSON_Data);
    if (pJSON_Wallets)      json_decref(pJSON_Wallets);
    if (pData)              ABC_WalletFreeData(pData);

    return cc;
}

/**
 * Sync the wallet's data
 */
tABC_CC ABC_WalletSyncData(Wallet &self, bool &dirty, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    std::string dir = self.dir();
    std::string syncDir = self.syncDir();
    tWalletData *pData      = NULL;

    // create the wallet root directory if necessary
    ABC_CHECK_NEW(fileEnsureDir(gContext->walletsDir()));

    // create the wallet directory - <Wallet_UUID1>  <- All data in this directory encrypted with MK_<Wallet_UUID1>
    ABC_CHECK_NEW(fileEnsureDir(dir));

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(self, &pData, pError));
    ABC_CHECK_ASSERT(NULL != pData->szWalletAcctKey, ABC_CC_Error, "Expected to find RepoAcctKey in key cache");

    // Either sync or clone, whichever is needed:
    if (!fileExists(syncDir))
    {
        ABC_CHECK_NEW(syncEnsureRepo(syncDir, dir + "tmp", pData->szWalletAcctKey));
        dirty = true;
    }
    else
    {
        ABC_CHECK_NEW(syncRepo(syncDir, pData->szWalletAcctKey, dirty));
    }
    if (dirty)
        ABC_WalletClearCache();

exit:
    return cc;
}

/**
 * Sets the name of a wallet
 */
tABC_CC ABC_WalletSetName(Wallet &self, const char *szName, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tWalletData *pData = NULL;
    std::string path;
    char *szJSON = NULL;

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(self, &pData, pError));

    // set the new name
    ABC_FREE_STR(pData->szName);
    ABC_STRDUP(pData->szName, szName);

    // create the json for the wallet name
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(szName, JSON_WALLET_NAME_FIELD, &szJSON, pError));
    //printf("name:\n%s\n", szJSON);

    // write the name out to the file
    path = self.syncDir() + WALLET_NAME_FILENAME;
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(
        U08Buf((unsigned char *)szJSON, strlen(szJSON) + 1),
        pData->MK, ABC_CryptoType_AES256, path.c_str(), pError));

exit:
    ABC_FREE_STR(szJSON);

    return cc;
}

/**
 * Sets the currency number of a wallet
 */
static
tABC_CC ABC_WalletSetCurrencyNum(Wallet &self, int currencyNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tWalletData *pData = NULL;
    std::string path;
    char *szJSON = NULL;

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(self, &pData, pError));

    // set the currency number
    pData->currencyNum = currencyNum;

    // create the json for the currency number
    ABC_CHECK_RET(ABC_UtilCreateIntJSONString(currencyNum, JSON_WALLET_CURRENCY_NUM_FIELD, &szJSON, pError));
    //printf("currency num:\n%s\n", szJSON);

    // write the name out to the file
    path = self.syncDir() + WALLET_CURRENCY_FILENAME;
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(
        U08Buf((unsigned char *)szJSON, strlen(szJSON) + 1),
        pData->MK, ABC_CryptoType_AES256, path.c_str(), pError));

exit:
    ABC_FREE_STR(szJSON);

    return cc;
}

/**
 * Adds the wallet data to the cache
 * If the wallet is not currently in the cache it is added
 */
static
tABC_CC ABC_WalletCacheData(Wallet &self, tWalletData **ppData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    tWalletData *pData = NULL;
    WalletJson json;
    DataChunk dataKey, bitcoinKey;

    // see if it is already in the cache
    ABC_CHECK_RET(ABC_WalletGetFromCacheByUUID(self.id().c_str(), &pData, pError));

    // if it is already cached
    if (NULL != pData)
    {
        // hang on to this data
        *ppData = pData;

        // if we don't do this, we'll free it below but it isn't ours, it is from the cache
        pData = NULL;
    }
    else
    {
        std::string syncDir = self.syncDir();
        // we need to add it

        // create a new wallet data struct
        ABC_NEW(pData, tWalletData);
        ABC_STRDUP(pData->szUUID, self.id().c_str());

        // Get the wallet info from the account:
        ABC_CHECK_NEW(self.account.wallets.json(json, self.id()));

        ABC_CHECK_NEW(json.dataKeyOk());
        ABC_CHECK_NEW(base16Decode(dataKey, json.dataKey()));
        ABC_BUF_DUP(pData->MK, toU08Buf(dataKey));

        ABC_CHECK_NEW(json.bitcoinKeyOk());
        ABC_CHECK_NEW(base16Decode(bitcoinKey, json.bitcoinKey()));
        ABC_BUF_DUP(pData->BitcoinPrivateSeed, toU08Buf(bitcoinKey));

        ABC_CHECK_NEW(json.syncKeyOk());
        ABC_STRDUP(pData->szWalletAcctKey, json.syncKey());

        // make sure this wallet exists, if it doesn't leave fields empty
        if (!fileExists(syncDir))
        {
            ABC_STRDUP(pData->szName, "");
            pData->currencyNum = -1;
        }
        else
        {
            std::string path;

            // get the name
            path = syncDir + WALLET_NAME_FILENAME;
            if (fileExists(path))
            {
                AutoU08Buf Data;
                ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(path.c_str(), pData->MK, &Data, pError));
                ABC_CHECK_RET(ABC_UtilGetStringValueFromJSONString((char *)Data.data(), JSON_WALLET_NAME_FIELD, &(pData->szName), pError));
            }
            else
            {
                ABC_STRDUP(pData->szName, "");
            }

            // get the currency num
            path = syncDir + WALLET_CURRENCY_FILENAME;
            if (fileExists(path))
            {
                AutoU08Buf Data;
                ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(path.c_str(), pData->MK, &Data, pError));
                ABC_CHECK_RET(ABC_UtilGetIntValueFromJSONString((char *)Data.data(), JSON_WALLET_CURRENCY_NUM_FIELD, (int *) &(pData->currencyNum), pError));
            }
            else
            {
                pData->currencyNum = -1;
            }
        }
        pData->balance = 0;
        pData->balanceDirty = true;

        // Add to cache
        ABC_CHECK_RET(ABC_WalletAddToCache(pData, pError));

        // hang on to this data
        *ppData = pData;

        // if we don't do this, we'll free it below but it isn't ours, it is from the cache
        pData = NULL;
    }

exit:
    if (pData)
    {
        ABC_WalletFreeData(pData);
        ABC_CLEAR_FREE(pData, sizeof(tWalletData));
    }

    return cc;
}

/**
 * Clears all the data from the cache
 */
void ABC_WalletClearCache()
{
    AutoCoreLock lock(gCoreMutex);

    if ((gWalletsCacheCount > 0) && (NULL != gaWalletsCacheArray))
    {
        for (unsigned i = 0; i < gWalletsCacheCount; i++)
        {
            tWalletData *pData = gaWalletsCacheArray[i];
            ABC_WalletFreeData(pData);
        }

        ABC_FREE(gaWalletsCacheArray);
        gWalletsCacheCount = 0;
    }
}

/**
 * Adds the given WalletDAta to the array of cached wallets
 */
static
tABC_CC ABC_WalletAddToCache(tWalletData *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    tWalletData *pExistingWalletData = NULL;

    ABC_CHECK_NULL(pData);

    // see if it exists first
    ABC_CHECK_RET(ABC_WalletGetFromCacheByUUID(pData->szUUID, &pExistingWalletData, pError));

    // if it doesn't currently exist in the array
    if (pExistingWalletData == NULL)
    {
        // if we don't have an array yet
        if ((gWalletsCacheCount == 0) || (NULL == gaWalletsCacheArray))
        {
            // create a new one
            gWalletsCacheCount = 0;
            ABC_ARRAY_NEW(gaWalletsCacheArray, 1, tWalletData*);
        }
        else
        {
            // extend the current one
            ABC_ARRAY_RESIZE(gaWalletsCacheArray, gWalletsCacheCount + 1, tWalletData*);
        }
        gaWalletsCacheArray[gWalletsCacheCount] = pData;
        gWalletsCacheCount++;
    }
    else
    {
        cc = ABC_CC_WalletAlreadyExists;
    }

exit:
    return cc;
}

/**
 * Remove from cache
 */
tABC_CC ABC_WalletRemoveFromCache(const char *szUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    bool bExists = false;
    unsigned i;

    ABC_CHECK_NULL(szUUID);

    for (i = 0; i < gWalletsCacheCount; ++i)
    {
        tWalletData *pWalletInfo = gaWalletsCacheArray[i];
        if (strcmp(pWalletInfo->szUUID, szUUID) == 0)
        {
            // Delete this element
            ABC_WalletFreeData(pWalletInfo);
            pWalletInfo = NULL;

            // put the last element in this elements place
            gaWalletsCacheArray[i] = gaWalletsCacheArray[gWalletsCacheCount - 1];
            gaWalletsCacheArray[gWalletsCacheCount - 1] = NULL;
            bExists = true;
            break;
        }
    }
    if (bExists)
    {
        // Reduce the count of cache
        gWalletsCacheCount--;
        // and resize
        ABC_ARRAY_RESIZE(gaWalletsCacheArray, gWalletsCacheCount, tWalletData*);
    }

exit:
    return cc;
}

/**
 * Searches for a wallet in the cached by UUID
 * if it is not found, the wallet data will be set to NULL
 */
static
tABC_CC ABC_WalletGetFromCacheByUUID(const char *szUUID, tWalletData **ppData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szUUID);
    ABC_CHECK_NULL(ppData);

    // assume we didn't find it
    *ppData = NULL;

    if ((gWalletsCacheCount > 0) && (NULL != gaWalletsCacheArray))
    {
        for (unsigned i = 0; i < gWalletsCacheCount; i++)
        {
            tWalletData *pData = gaWalletsCacheArray[i];
            if (0 == strcmp(szUUID, pData->szUUID))
            {
                // found it
                *ppData = pData;
                break;
            }
        }
    }

exit:

    return cc;
}

/**
 * Free's the given wallet data elements
 */
static
void ABC_WalletFreeData(tWalletData *pData)
{
    if (pData)
    {
        ABC_FREE_STR(pData->szUUID);

        ABC_FREE_STR(pData->szName);

        pData->currencyNum = -1;

        ABC_BUF_FREE(pData->MK);

        ABC_BUF_FREE(pData->BitcoinPrivateSeed);
    }
}

tABC_CC ABC_WalletDirtyCache(Wallet &self,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    tWalletData     *pData = NULL;

    ABC_CHECK_RET(ABC_WalletCacheData(self, &pData, pError));
    pData->balanceDirty = true;
exit:
    return cc;
}

/**
 * Gets information on the given wallet.
 *
 * This function allocates and fills in an wallet info structure with the information
 * associated with the given wallet UUID
 *
 * @param ppWalletInfo          Pointer to store the pointer of the allocated wallet info struct
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_WalletGetInfo(Wallet &self,
                          tABC_WalletInfo **ppWalletInfo,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    tWalletData     *pData = NULL;
    tABC_WalletInfo *pInfo = NULL;
    tABC_TxInfo     **aTransactions = NULL;
    unsigned int    nTxCount = 0;

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(self, &pData, pError));

    // create the wallet info struct
    ABC_NEW(pInfo, tABC_WalletInfo);

    // copy data from what was cachqed
    ABC_STRDUP(pInfo->szUUID, self.id().c_str());
    if (pData->szName != NULL)
    {
        ABC_STRDUP(pInfo->szName, pData->szName);
    }
    pInfo->currencyNum = pData->currencyNum;
    {
        bool archived;
        ABC_CHECK_NEW(self.account.wallets.archived(archived, self.id()));
        pInfo->archived = archived;
    }

    if (pData->balanceDirty)
    {
        ABC_CHECK_RET(
            ABC_TxGetTransactions(self,
                                  ABC_GET_TX_ALL_TIMES, ABC_GET_TX_ALL_TIMES,
                                  &aTransactions, &nTxCount, pError));
        ABC_CHECK_RET(ABC_BridgeFilterTransactions(self, aTransactions, &nTxCount, pError));
        pData->balance = 0;
        for (unsigned i = 0; i < nTxCount; i++)
        {
            tABC_TxInfo *pTxInfo = aTransactions[i];
            pData->balance += pTxInfo->pDetails->amountSatoshi;
        }
        pData->balanceDirty = false;
    }
    pInfo->balanceSatoshi = pData->balance;


    // assign it to the user's pointer
    *ppWalletInfo = pInfo;
    pInfo = NULL;

exit:
    ABC_CLEAR_FREE(pInfo, sizeof(tABC_WalletInfo));
    if (nTxCount > 0) ABC_FreeTransactions(aTransactions, nTxCount);

    return cc;
}

/**
 * Free the wallet info.
 *
 * This function frees the info struct returned from ABC_WalletGetInfo.
 *
 * @param pWalletInfo   Wallet info to be free'd
 */
void ABC_WalletFreeInfo(tABC_WalletInfo *pWalletInfo)
{
    if (pWalletInfo != NULL)
    {
        ABC_FREE_STR(pWalletInfo->szUUID);
        ABC_FREE_STR(pWalletInfo->szName);

        ABC_CLEAR_FREE(pWalletInfo, sizeof(sizeof(tABC_WalletInfo)));
    }
}

} // namespace abcds
