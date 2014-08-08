/**
 * @file
 * AirBitz Wallet functions.
 *
 * This file contains all of the functions associated with wallet creation,
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
#include <unistd.h>
#include "ABC_Wallet.h"
#include "ABC_Tx.h"
#include "ABC_Account.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Crypto.h"
#include "ABC_Login.h"
#include "ABC_Mutex.h"
#include "ABC_ServerDefs.h"
#include "ABC_Sync.h"
#include "ABC_URL.h"

#define WALLET_KEY_LENGTH                       AES_256_KEY_LENGTH

#define WALLET_BITCOIN_PRIVATE_SEED_LENGTH      32

#define ABC_SERVER_JSON_REPO_WALLET_FIELD       "repo_wallet_key"
#define ABC_SERVER_JSON_EREPO_WALLET_FIELD      "erepo_wallet_key"

#define WALLET_DIR                              "Wallets"
#define WALLET_SYNC_DIR                         "sync"
#define WALLET_TX_DIR                           "Transactions"
#define WALLET_ADDR_DIR                         "Addresses"
#define WALLET_ACCOUNTS_WALLETS_FILENAME        "Wallets.json"
#define WALLET_NAME_FILENAME                    "WalletName.json"
#define WALLET_CURRENCY_FILENAME                "Currency.json"
#define WALLET_ACCOUNTS_FILENAME                "Accounts.json"

#define JSON_WALLET_WALLETS_FIELD               "wallets"
#define JSON_WALLET_NAME_FIELD                  "walletName"
#define JSON_WALLET_ATTRIBUTES_FIELD            "attributes"
#define JSON_WALLET_CURRENCY_NUM_FIELD          "num"
#define JSON_WALLET_ACCOUNTS_FIELD              "accounts"

// holds wallet data (including keys) for a given wallet
typedef struct sWalletData
{
    char            *szUUID;
    char            *szName;
    char            *szUserName;
    char            *szPassword;
    char            *szWalletDir;
    char            *szWalletSyncDir;
    char            *szWalletAcctKey;
    int             currencyNum;
    unsigned int    numAccounts;
    char            **aszAccounts;
    tABC_U08Buf     MK;
    tABC_U08Buf     BitcoinPrivateSeed;
    unsigned        archived;
} tWalletData;

// this holds all the of the currently cached wallets
static unsigned int gWalletsCacheCount = 0;
static tWalletData **gaWalletsCacheArray = NULL;

static tABC_CC ABC_WalletServerRepoPost(tABC_U08Buf L1, tABC_U08Buf LP1, const char *szRepoAcctKey, const char *szPath, tABC_Error *pError);
static tABC_CC ABC_WalletSetCurrencyNum(const char *szUserName, const char *szPassword, const char *szUUID, int currencyNum, tABC_Error *pError);
static tABC_CC ABC_WalletAddAccount(const char *szUserName, const char *szPassword, const char *szUUID, const char *szAccount, tABC_Error *pError);
static tABC_CC ABC_WalletCreateRootDir(tABC_Error *pError);
static tABC_CC ABC_WalletGetRootDirName(char **pszRootDir, tABC_Error *pError);
static tABC_CC ABC_WalletGetSyncDirName(char **pszDir, const char *szWalletUUID, tABC_Error *pError);
static tABC_CC ABC_WalletCacheData(const char *szUserName, const char *szPassword, const char *szUUID, tWalletData **ppData, tABC_Error *pError);
static tABC_CC ABC_WalletAddToCache(tWalletData *pData, tABC_Error *pError);
static tABC_CC ABC_WalletGetFromCacheByUUID(const char *szUUID, tWalletData **ppData, tABC_Error *pError);
static void    ABC_WalletFreeData(tWalletData *pData);
static tABC_CC ABC_WalletMutexLock(tABC_Error *pError);
static tABC_CC ABC_WalletMutexUnlock(tABC_Error *pError);

/**
 * Allocates the wallet create info structure and
 * populates it with the data given
 */
tABC_CC ABC_WalletCreateInfoAlloc(tABC_WalletCreateInfo **ppWalletCreateInfo,
                                  const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletName,
                                  int        currencyNum,
                                  unsigned int attributes,
                                  tABC_Request_Callback fRequestCallback,
                                  void *pData,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(ppWalletCreateInfo);
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szWalletName);
    /* ABC_CHECK_NULL(fRequestCallback); */

    tABC_WalletCreateInfo *pWalletCreateInfo;
    ABC_ALLOC(pWalletCreateInfo, sizeof(tABC_WalletCreateInfo));

    ABC_STRDUP(pWalletCreateInfo->szUserName, szUserName);
    ABC_STRDUP(pWalletCreateInfo->szPassword, szPassword);
    ABC_STRDUP(pWalletCreateInfo->szWalletName, szWalletName);
    pWalletCreateInfo->currencyNum = currencyNum;
    pWalletCreateInfo->attributes = attributes;

    pWalletCreateInfo->fRequestCallback = fRequestCallback;

    pWalletCreateInfo->pData = pData;

    *ppWalletCreateInfo = pWalletCreateInfo;

exit:

    return cc;
}

/**
 * Frees the wallet creation info structure
 */
void ABC_WalletCreateInfoFree(tABC_WalletCreateInfo *pWalletCreateInfo)
{
    if (pWalletCreateInfo)
    {
        ABC_FREE_STR(pWalletCreateInfo->szUserName);
        ABC_FREE_STR(pWalletCreateInfo->szPassword);
        ABC_FREE_STR(pWalletCreateInfo->szWalletName);

        ABC_CLEAR_FREE(pWalletCreateInfo, sizeof(tABC_WalletCreateInfo));
    }
}

/**
 * Create a new wallet. Assumes it is running in a thread.
 *
 * This function creates a new wallet.
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to create a wallet (should be a tABC_WalletCreateInfo)
 */
void *ABC_WalletCreateThreaded(void *pData)
{
    tABC_WalletCreateInfo *pInfo = (tABC_WalletCreateInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;
        memset(&results, 0, sizeof(tABC_RequestResults));

        results.requestType = ABC_RequestType_CreateWallet;

        results.bSuccess = false;

        // create the wallet
        tABC_CC CC = ABC_WalletCreate(pInfo, (char **) &(results.pRetData), &(results.errorInfo));
        results.errorInfo.code = CC;

        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);

        // it is our responsibility to free the info struct
        ABC_WalletCreateInfoFree(pInfo);
    }

    return NULL;
}

/**
 * Creates the wallet with the given info.
 *
 * @param pInfo Pointer to wallet information
 * @param pszUUID Pointer to hold allocated pointer to UUID string
 */
tABC_CC ABC_WalletCreate(tABC_WalletCreateInfo *pInfo,
                         char                  **pszUUID,
                         tABC_Error            *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szFilename       = NULL;
    char *szJSON           = NULL;
    char *szUUID           = NULL;
    char *szRepoURL        = NULL;
    char *szWalletDir      = NULL;
    json_t *pJSON_Data     = NULL;
    json_t *pJSON_Wallets  = NULL;
    tABC_SyncKeys *pKeys   = NULL;
    tABC_U08Buf L1            = ABC_BUF_NULL;
    tABC_U08Buf LP1           = ABC_BUF_NULL;
    tABC_U08Buf WalletAcctKey = ABC_BUF_NULL;

    tWalletData *pData = NULL;

    ABC_CHECK_NULL(pInfo);
    ABC_CHECK_NULL(pszUUID);

    // create a new wallet data struct
    ABC_ALLOC(pData, sizeof(tWalletData));
    ABC_STRDUP(pData->szUserName, pInfo->szUserName);
    ABC_STRDUP(pData->szPassword, pInfo->szPassword);
    pData->archived = 0;

    // get L1
    ABC_CHECK_RET(ABC_LoginGetKey(pData->szUserName, pData->szPassword, ABC_LoginKey_L1, &L1, pError));

    // get LP1
    ABC_CHECK_RET(ABC_LoginGetKey(pData->szUserName, pData->szPassword, ABC_LoginKey_LP1, &LP1, pError));

    // create wallet guid
    ABC_CHECK_RET(ABC_CryptoGenUUIDString(&szUUID, pError));
    ABC_STRDUP(pData->szUUID, szUUID);
    ABC_STRDUP(*pszUUID, szUUID);

    // generate the master key for this wallet - MK_<Wallet_GUID1>
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(WALLET_KEY_LENGTH, &pData->MK, pError));

    // create and set the bitcoin private seed for this wallet
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(WALLET_BITCOIN_PRIVATE_SEED_LENGTH, &pData->BitcoinPrivateSeed, pError));

    // Create Wallet Repo key
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(SYNC_KEY_LENGTH, &WalletAcctKey, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(WalletAcctKey, &(pData->szWalletAcctKey), pError));

    // create the wallet root directory if necessary
    ABC_CHECK_RET(ABC_WalletCreateRootDir(pError));

    // create the wallet directory - <Wallet_UUID1>  <- All data in this directory encrypted with MK_<Wallet_UUID1>
    ABC_CHECK_RET(ABC_WalletGetDirName(&(pData->szWalletDir), pData->szUUID, pError));
    ABC_CHECK_RET(ABC_FileIOCreateDir(pData->szWalletDir, pError));
    ABC_STRDUP(szWalletDir, pData->szWalletDir);

    // create the wallet sync dir under the main dir
    ABC_CHECK_RET(ABC_WalletGetSyncDirName(&(pData->szWalletSyncDir), pData->szUUID, pError));
    ABC_CHECK_RET(ABC_FileIOCreateDir(pData->szWalletSyncDir, pError));

    // we now have a new wallet so go ahead and cache its data
    ABC_CHECK_RET(ABC_WalletAddToCache(pData, pError));

    // all the functions below assume the wallet is in the cache or can be loaded into the cache
    // set the wallet name
    ABC_CHECK_RET(ABC_WalletSetName(pInfo->szUserName, pInfo->szPassword, szUUID, pInfo->szWalletName, pError));

    // set the currency
    ABC_CHECK_RET(ABC_WalletSetCurrencyNum(pInfo->szUserName, pInfo->szPassword, szUUID, pInfo->currencyNum, pError));

    // Request remote wallet repo
    ABC_CHECK_RET(ABC_WalletServerRepoPost(L1, LP1, pData->szWalletAcctKey,
                                           ABC_SERVER_WALLET_CREATE_PATH, pError));

    // set this account for the wallet's first account
    ABC_CHECK_RET(ABC_WalletAddAccount(pInfo->szUserName, pInfo->szPassword, szUUID, pInfo->szUserName, pError));

    // TODO: should probably add the creation date to optimize wallet export (assuming it is even used)

    // Create Repo URL
    ABC_CHECK_RET(ABC_SyncGetServer(pData->szWalletAcctKey, &szRepoURL, pError));

    ABC_DebugLog("Wallet Repo: %s %s\n", pData->szWalletSyncDir, szRepoURL);

    // Init the git repo and sync it
    int dirty;
    ABC_CHECK_RET(ABC_SyncMakeRepo(pData->szWalletSyncDir, pError));
    ABC_CHECK_RET(ABC_SyncRepo(pData->szWalletSyncDir, szRepoURL, &dirty, pError));

    // Actiate the remote wallet
    ABC_CHECK_RET(ABC_WalletServerRepoPost(L1, LP1, pData->szWalletAcctKey,
                                           ABC_SERVER_WALLET_ACTIVATE_PATH, pError));

    // If everything worked, add the wallet to the account:
    tABC_AccountWalletInfo info; // No need to free this
    info.szUUID = szUUID;
    info.MK = pData->MK;
    info.BitcoinSeed = pData->BitcoinPrivateSeed;
    info.SyncKey = WalletAcctKey;
    info.archived = 0;
    ABC_CHECK_RET(ABC_LoginGetSyncKeys(pData->szUserName, pData->szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountWalletList(pKeys, NULL, &info.sortIndex, pError));
    ABC_CHECK_RET(ABC_AccountWalletSave(pKeys, &info, pError));

    // Now the wallet is written to disk, generate some addresses
    ABC_CHECK_RET(ABC_TxCreateInitialAddresses(
                    pData->szUserName, pData->szPassword,
                    pData->szUUID, pError));

    // After wallet is created, sync the account, ignoring any errors
    tABC_Error Error;
    ABC_CHECK_RET(ABC_LoginSyncData(pInfo->szUserName, pInfo->szPassword, &dirty, &Error));

    pData = NULL; // so we don't free what we just added to the cache
exit:
    if (cc != ABC_CC_Ok)
    {
        if (szUUID)
        {
            ABC_WalletRemoveFromCache(szUUID, NULL);
        }
        if (szWalletDir)
        {
            ABC_FileIODeleteRecursive(szWalletDir, NULL);
        }
    }
    ABC_FREE_STR(szWalletDir);
    ABC_FREE_STR(szRepoURL);
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szJSON);
    ABC_FREE_STR(szUUID);
    if (pKeys)              ABC_SyncFreeKeys(pKeys);
    if (pJSON_Data)         json_decref(pJSON_Data);
    if (pJSON_Wallets)      json_decref(pJSON_Wallets);
    if (pData)              ABC_WalletFreeData(pData);

    return cc;
}

tABC_CC ABC_WalletSyncAll(const char *szUserName, const char *szPassword, int *pDirty, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char **aszUUIDs                = NULL;
    char *szDirectory              = NULL;
    char *szSyncDirectory          = NULL;
    tABC_GeneralInfo *pInfo        = NULL;
    tABC_SyncKeys *pKeys           = NULL;
    int dirty           = 0;
    unsigned int i      = 0;
    unsigned int nUUIDs = 0;

    // Its not dirty...yet
    *pDirty = 0;

    // Fetch general info
    ABC_CHECK_RET(ABC_GeneralGetInfo(&pInfo, pError));

    // Get the wallet list
    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountWalletList(pKeys, &aszUUIDs, &nUUIDs, pError));

    // create the wallet root directory if necessary
    ABC_CHECK_RET(ABC_WalletCreateRootDir(pError));

    for (i = 0; i < nUUIDs; ++i)
    {
        char *szUUID = aszUUIDs[i];
        bool bExists = false;

        // create the wallet directory - <Wallet_UUID1>  <- All data in this directory encrypted with MK_<Wallet_UUID1>
        ABC_CHECK_RET(ABC_WalletGetDirName(&szDirectory, szUUID, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szDirectory, &bExists, pError));
        if (!bExists)
        {
            ABC_CHECK_RET(ABC_FileIOCreateDir(szDirectory, pError));
            ABC_FREE_STR(szDirectory);
        }

        // create the wallet sync dir under the main dir
        ABC_CHECK_RET(ABC_WalletGetSyncDirName(&szSyncDirectory, szUUID, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szSyncDirectory, &bExists, pError));
        if (!bExists)
        {
            ABC_CHECK_RET(ABC_FileIOCreateDir(szSyncDirectory, pError));

            // Init repo
            ABC_CHECK_RET(ABC_SyncMakeRepo(szSyncDirectory, pError));
            ABC_FREE_STR(szSyncDirectory);
        }

        // Sync Wallet
        dirty = 0;
        ABC_CHECK_RET(ABC_WalletSyncData(szUserName, szPassword, szUUID, pInfo, &dirty, pError));
        if (dirty)
        {
            *pDirty = 1;
        }
    }
exit:
    ABC_UtilFreeStringArray(aszUUIDs, nUUIDs);
    ABC_FREE_STR(szDirectory);
    ABC_FREE_STR(szSyncDirectory);
    ABC_SyncFreeKeys(pKeys);

    return cc;
}

/**
 * Sync the wallet's data
 */
tABC_CC ABC_WalletSyncData(const char *szUserName, const char *szPassword, const char *szUUID,
                           tABC_GeneralInfo *pInfo, int *pDirty, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szRepoURL = NULL;
    tWalletData *pData = NULL;

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(szUserName, szPassword, szUUID, &pData, pError));
    ABC_CHECK_ASSERT(NULL != pData->szWalletAcctKey, ABC_CC_Error, "Expected to find RepoAcctKey in key cache");

    // Create Repo URL
    ABC_CHECK_RET(ABC_SyncGetServer(pData->szWalletAcctKey, &szRepoURL, pError));

    ABC_DebugLog("Wallet Repo: %s %s\n", pData->szWalletSyncDir, szRepoURL);

    // Sync
    ABC_CHECK_RET(ABC_SyncRepo(pData->szWalletSyncDir, szRepoURL, pDirty, pError));
exit:
    ABC_FREE_STR(szRepoURL);
    return cc;
}

/**
 * Creates an git repo on the server.
 *
 * @param L1   Login hash for the account
 * @param LP1  Password hash for the account
 */
static
tABC_CC ABC_WalletServerRepoPost(tABC_U08Buf L1,
                                 tABC_U08Buf LP1,
                                 const char *szWalletAcctKey,
                                 const char *szPath,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szURL     = NULL;
    char *szResults = NULL;
    char *szPost    = NULL;
    char *szL1_Base64 = NULL;
    char *szLP1_Base64 = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL_BUF(LP1);

    // create the URL
    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, szPath);

    // create base64 versions of L1 and LP1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szLP1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{ssssss}",
                        ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                        ABC_SERVER_JSON_LP1_FIELD, szLP1_Base64,
                        ABC_SERVER_JSON_REPO_WALLET_FIELD, szWalletAcctKey);
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %s", szURL, szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %s", szResults);

    ABC_CHECK_RET(ABC_URLCheckResults(szResults, NULL, pError));
exit:
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szResults);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szL1_Base64);
    ABC_FREE_STR(szLP1_Base64);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

/**
 * Sets the name of a wallet
 */
tABC_CC ABC_WalletSetName(const char *szUserName, const char *szPassword, const char *szUUID, const char *szName, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tWalletData *pData = NULL;
    char *szFilename = NULL;
    char *szJSON = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szUUID);
    ABC_CHECK_NULL(szName);

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(szUserName, szPassword, szUUID, &pData, pError));

    // set the new name
    ABC_FREE_STR(pData->szName);
    ABC_STRDUP(pData->szName, szName);

    // create the json for the wallet name
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(szName, JSON_WALLET_NAME_FIELD, &szJSON, pError));
    //printf("name:\n%s\n", szJSON);

    // write the name out to the file
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_NAME_FILENAME);
    tABC_U08Buf Data;
    ABC_BUF_SET_PTR(Data, (unsigned char *)szJSON, strlen(szJSON) + 1);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(Data, pData->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szJSON);

    return cc;
}

/**
 * Sets the currency number of a wallet
 */
static
tABC_CC ABC_WalletSetCurrencyNum(const char *szUserName, const char *szPassword, const char *szUUID, int currencyNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tWalletData *pData = NULL;
    char *szFilename = NULL;
    char *szJSON = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szUUID);

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(szUserName, szPassword, szUUID, &pData, pError));

    // set the currency number
    pData->currencyNum = currencyNum;

    // create the json for the currency number
    ABC_CHECK_RET(ABC_UtilCreateIntJSONString(currencyNum, JSON_WALLET_CURRENCY_NUM_FIELD, &szJSON, pError));
    //printf("currency num:\n%s\n", szJSON);

    // write the name out to the file
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_CURRENCY_FILENAME);
    tABC_U08Buf Data;
    ABC_BUF_SET_PTR(Data, (unsigned char *)szJSON, strlen(szJSON) + 1);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(Data, pData->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szJSON);

    return cc;
}

/**
 * Adds the given account to the list of accounts that uses this wallet
 */
static
tABC_CC ABC_WalletAddAccount(const char *szUserName, const char *szPassword, const char *szUUID, const char *szAccount, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tWalletData *pData = NULL;
    char *szFilename = NULL;
    json_t *dataJSON = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szUUID);
    ABC_CHECK_NULL(szAccount);

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(szUserName, szPassword, szUUID, &pData, pError));

    // if there are already accounts in the list
    if ((pData->aszAccounts) && (pData->numAccounts > 0))
    {
        pData->aszAccounts = realloc(pData->aszAccounts, sizeof(char *) * (pData->numAccounts + 1));
    }
    else
    {
        pData->numAccounts = 0;
        ABC_ALLOC(pData->aszAccounts, sizeof(char *));
    }
    ABC_STRDUP(pData->aszAccounts[pData->numAccounts], szAccount);
    pData->numAccounts++;

    // create the json for the accounts
    ABC_CHECK_RET(ABC_UtilCreateArrayJSONObject(pData->aszAccounts, pData->numAccounts, JSON_WALLET_ACCOUNTS_FIELD, &dataJSON, pError));

    // write the name out to the file
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_ACCOUNTS_FILENAME);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(dataJSON, pData->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    ABC_FREE_STR(szFilename);
    if (dataJSON)       json_decref(dataJSON);

    return cc;
}

/**
 * creates the wallet directory if needed
 */
static
tABC_CC ABC_WalletCreateRootDir(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletRoot = NULL;

    // create the account directory string
    ABC_CHECK_RET(ABC_WalletGetRootDirName(&szWalletRoot, pError));

    // if it doesn't exist
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szWalletRoot, &bExists, pError));
    if (true != bExists)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szWalletRoot, pError));
    }

exit:
    ABC_FREE_STR(szWalletRoot);

    return cc;
}

/**
 * Gets the root directory for the wallets
 * the string is allocated so it is up to the caller to free it
 */
static
tABC_CC ABC_WalletGetRootDirName(char **pszRootDir, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szFileIORootDir = NULL;

    ABC_CHECK_NULL(pszRootDir);

    ABC_CHECK_RET(ABC_FileIOGetRootDir(&szFileIORootDir, pError));

    // create the wallet directory string
    ABC_ALLOC(*pszRootDir, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszRootDir, "%s/%s", szFileIORootDir, WALLET_DIR);

exit:
    ABC_FREE_STR(szFileIORootDir);

    return cc;
}

/**
 * Gets the directory for the given wallet UUID
 * the string is allocated so it is up to the caller to free it
 */
tABC_CC ABC_WalletGetDirName(char **pszDir, const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletRootDir = NULL;

    ABC_CHECK_NULL(pszDir);

    ABC_CHECK_RET(ABC_WalletGetRootDirName(&szWalletRootDir, pError));

    // create the account directory string
    ABC_ALLOC(*pszDir, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_NULL(*pszDir);
    sprintf(*pszDir, "%s/%s", szWalletRootDir, szWalletUUID);

exit:
    ABC_FREE_STR(szWalletRootDir);

    return cc;
}

/**
 * Gets the sync directory for the given wallet UUID.
 *
 * @param pszDir the output directory name. The caller must free this.
 */
static
tABC_CC ABC_WalletGetSyncDirName(char **pszDir, const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletDir = NULL;

    ABC_CHECK_NULL(pszDir);
    ABC_CHECK_NULL(szWalletUUID);

    ABC_CHECK_RET(ABC_WalletGetDirName(&szWalletDir, szWalletUUID, pError));

    ABC_ALLOC(*pszDir, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_NULL(*pszDir);
    sprintf(*pszDir, "%s/%s", szWalletDir, WALLET_SYNC_DIR);

exit:
    ABC_FREE_STR(szWalletDir);

    return cc;
}

/**
 * Gets the transaction directory for the given wallet UUID.
 *
 * @param pszDir the output directory name. The caller must free this.
 */
tABC_CC ABC_WalletGetTxDirName(char **pszDir, const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletSyncDir = NULL;

    ABC_CHECK_NULL(pszDir);
    ABC_CHECK_NULL(szWalletUUID);

    ABC_CHECK_RET(ABC_WalletGetSyncDirName(&szWalletSyncDir, szWalletUUID, pError));

    ABC_ALLOC(*pszDir, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_NULL(*pszDir);
    sprintf(*pszDir, "%s/%s", szWalletSyncDir, WALLET_TX_DIR);

exit:
    ABC_FREE_STR(szWalletSyncDir);

    return cc;
}

/**
 * Gets the address directory for the given wallet UUID.
 *
 * @param pszDir the output directory name. The caller must free this.
 */
tABC_CC ABC_WalletGetAddressDirName(char **pszDir, const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletSyncDir = NULL;

    ABC_CHECK_NULL(pszDir);
    ABC_CHECK_NULL(szWalletUUID);

    ABC_CHECK_RET(ABC_WalletGetSyncDirName(&szWalletSyncDir, szWalletUUID, pError));

    ABC_ALLOC(*pszDir, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_NULL(*pszDir);
    sprintf(*pszDir, "%s/%s", szWalletSyncDir, WALLET_ADDR_DIR);

exit:
    ABC_FREE_STR(szWalletSyncDir);

    return cc;
}

/**
 * Adds the wallet data to the cache
 * If the wallet is not currently in the cache it is added
 */
static
tABC_CC ABC_WalletCacheData(const char *szUserName, const char *szPassword, const char *szUUID, tWalletData **ppData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tWalletData *pData = NULL;
    char *szFilename = NULL;
    tABC_U08Buf Data = ABC_BUF_NULL;
    tABC_SyncKeys *pKeys = NULL;
    tABC_AccountWalletInfo info;
    memset(&info, 0, sizeof(tABC_AccountWalletInfo));

    ABC_CHECK_RET(ABC_WalletMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szUUID);

    // see if it is already in the cache
    ABC_CHECK_RET(ABC_WalletGetFromCacheByUUID(szUUID, &pData, pError));

    // if it is already cached
    if (NULL != pData)
    {
        // hang on to this data
        *ppData = pData;

        // if we don't do this, we'll free it below but it isn't ours, it is from the cache
        pData = NULL;

        // if the username and password doesn't match
        if ((0 != strcmp((*ppData)->szUserName, szUserName)) ||
            (0 != strcmp((*ppData)->szPassword, szPassword)))
        {
            *ppData = NULL;
            ABC_RET_ERROR(ABC_CC_Error, "Incorrect username and password for wallet UUID");
        }
    }
    else
    {
        // we need to add it

        // create a new wallet data struct
        ABC_ALLOC(pData, sizeof(tWalletData));
        ABC_STRDUP(pData->szUserName, szUserName);
        ABC_STRDUP(pData->szPassword, szPassword);
        ABC_STRDUP(pData->szUUID, szUUID);
        ABC_CHECK_RET(ABC_WalletGetDirName(&(pData->szWalletDir), szUUID, pError));
        ABC_CHECK_RET(ABC_WalletGetSyncDirName(&(pData->szWalletSyncDir), szUUID, pError));

        // make sure this wallet exists
        bool bExists = false;
        ABC_CHECK_RET(ABC_FileIOFileExists(pData->szWalletSyncDir, &bExists, pError));
        ABC_CHECK_ASSERT(bExists == true, ABC_CC_InvalidWalletID, "Wallet does not exist");

        // Get the wallet info from the account:
        ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
        ABC_CHECK_RET(ABC_AccountWalletLoad(pKeys, szUUID, &info, pError));
        pData->archived = info.archived;

        // Steal the wallet info into our struct:
        pData->MK = info.MK;
        info.MK.p = NULL;

        // Steal the bitcoin seed into our struct:
        pData->BitcoinPrivateSeed = info.BitcoinSeed;
        info.BitcoinSeed.p = NULL;

        // Encode the sync key into our struct:
        ABC_CHECK_RET(ABC_CryptoHexEncode(info.SyncKey, &(pData->szWalletAcctKey), pError));

        // get the name
        ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
        sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_NAME_FILENAME);
        bExists = false;
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
        if (true == bExists)
        {
            ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, pData->MK, &Data, pError));
            ABC_CHECK_RET(ABC_UtilGetStringValueFromJSONString((char *)ABC_BUF_PTR(Data), JSON_WALLET_NAME_FIELD, &(pData->szName), pError));
            ABC_BUF_FREE(Data);
        }
        else
        {
            ABC_STRDUP(pData->szName, "");
        }

        // get the currency num
        sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_CURRENCY_FILENAME);
        bExists = false;
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
        if (true == bExists)
        {
            ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, pData->MK, &Data, pError));
            ABC_CHECK_RET(ABC_UtilGetIntValueFromJSONString((char *)ABC_BUF_PTR(Data), JSON_WALLET_CURRENCY_NUM_FIELD, (int *) &(pData->currencyNum), pError));
            ABC_BUF_FREE(Data);
        }
        else
        {
            pData->currencyNum = -1;
        }

        // get the accounts
        sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_ACCOUNTS_FILENAME);
        bExists = false;
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
        if (true == bExists)
        {
            ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, pData->MK, &Data, pError));
            ABC_CHECK_RET(ABC_UtilGetArrayValuesFromJSONString((char *)ABC_BUF_PTR(Data), JSON_WALLET_ACCOUNTS_FIELD, &(pData->aszAccounts), &(pData->numAccounts), pError));
            ABC_BUF_FREE(Data);
        }
        else
        {
            pData->numAccounts = 0;
            pData->aszAccounts = NULL;
        }

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
    ABC_FREE_STR(szFilename);
    ABC_BUF_FREE(Data);
    ABC_SyncFreeKeys(pKeys);
    ABC_AccountWalletInfoFree(&info);

    ABC_WalletMutexUnlock(NULL);
    return cc;
}

/**
 * Clears all the data from the cache
 */
tABC_CC ABC_WalletClearCache(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_WalletMutexLock(pError));

    if ((gWalletsCacheCount > 0) && (NULL != gaWalletsCacheArray))
    {
        for (int i = 0; i < gWalletsCacheCount; i++)
        {
            tWalletData *pData = gaWalletsCacheArray[i];
            ABC_WalletFreeData(pData);
        }

        ABC_FREE(gaWalletsCacheArray);
        gWalletsCacheCount = 0;
    }

exit:

    ABC_WalletMutexUnlock(NULL);
    return cc;
}

/**
 * Adds the given WalletDAta to the array of cached wallets
 */
static
tABC_CC ABC_WalletAddToCache(tWalletData *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_WalletMutexLock(pError));
    ABC_CHECK_NULL(pData);

    // see if it exists first
    tWalletData *pExistingWalletData = NULL;
    ABC_CHECK_RET(ABC_WalletGetFromCacheByUUID(pData->szUUID, &pExistingWalletData, pError));

    // if it doesn't currently exist in the array
    if (pExistingWalletData == NULL)
    {
        // if we don't have an array yet
        if ((gWalletsCacheCount == 0) || (NULL == gaWalletsCacheArray))
        {
            // create a new one
            gWalletsCacheCount = 0;
            ABC_ALLOC(gaWalletsCacheArray, sizeof(tWalletData *));
        }
        else
        {
            // extend the current one
            gaWalletsCacheArray = realloc(gaWalletsCacheArray, sizeof(tWalletData *) * (gWalletsCacheCount + 1));

        }
        gaWalletsCacheArray[gWalletsCacheCount] = pData;
        gWalletsCacheCount++;
    }
    else
    {
        cc = ABC_CC_WalletAlreadyExists;
    }

exit:

    ABC_WalletMutexUnlock(NULL);
    return cc;
}

/**
 * Remove from cache
 */
tABC_CC ABC_WalletRemoveFromCache(const char *szUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int i;

    ABC_CHECK_RET(ABC_WalletMutexLock(pError));
    ABC_CHECK_NULL(szUUID);

    bool bExists = false;
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
        gaWalletsCacheArray = realloc(gaWalletsCacheArray, sizeof(tWalletData *) * gWalletsCacheCount);
    }

exit:

    ABC_WalletMutexUnlock(NULL);
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
        for (int i = 0; i < gWalletsCacheCount; i++)
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

        ABC_FREE_STR(pData->szUserName);

        ABC_FREE_STR(pData->szPassword);

        ABC_FREE_STR(pData->szWalletDir);

        ABC_FREE_STR(pData->szWalletSyncDir);

        pData->archived = 0;
        pData->currencyNum = -1;

        ABC_UtilFreeStringArray(pData->aszAccounts, pData->numAccounts);
        pData->aszAccounts = NULL;

        ABC_BUF_FREE(pData->MK);

        ABC_BUF_FREE(pData->BitcoinPrivateSeed);
    }
}

/**
 * Gets information on the given wallet.
 *
 * This function allocates and fills in an wallet info structure with the information
 * associated with the given wallet UUID
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param szUUID                UUID of the wallet
 * @param ppWalletInfo          Pointer to store the pointer of the allocated wallet info struct
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_WalletGetInfo(const char *szUserName,
                          const char *szPassword,
                          const char *szUUID,
                          tABC_WalletInfo **ppWalletInfo,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    int i;
    tWalletData     *pData = NULL;
    tABC_WalletInfo *pInfo = NULL;
    tABC_TxInfo **aTransactions = NULL;
    unsigned int nCount = 0;

    ABC_CHECK_RET(ABC_WalletMutexLock(pError));

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szUUID);
    ABC_CHECK_NULL(ppWalletInfo);

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(szUserName, szPassword, szUUID, &pData, pError));

    // create the wallet info struct
    ABC_ALLOC(pInfo, sizeof(tABC_WalletInfo));

    // copy data from what was cached
    ABC_STRDUP(pInfo->szUUID, szUUID);
    if (pData->szName != NULL)
    {
        ABC_STRDUP(pInfo->szName, pData->szName);
    }
    if (pData->szUserName != NULL)
    {
        ABC_STRDUP(pInfo->szUserName, pData->szUserName);
    }
    pInfo->currencyNum = pData->currencyNum;
    pInfo->archived  = pData->archived;

    ABC_CHECK_RET(
        ABC_GetTransactions(szUserName, szPassword, szUUID,
                            &aTransactions, &nCount, pError));
    pInfo->balanceSatoshi = 0;
    for (i = 0; i < nCount; i++)
    {
        tABC_TxInfo *pTxInfo = aTransactions[i];
        pInfo->balanceSatoshi += pTxInfo->pDetails->amountSatoshi;
    }

    // assign it to the user's pointer
    *ppWalletInfo = pInfo;
    pInfo = NULL;

exit:
    ABC_CLEAR_FREE(pInfo, sizeof(tABC_WalletInfo));
    ABC_FreeTransactions(aTransactions, nCount);

    ABC_CHECK_RET(ABC_WalletMutexUnlock(pError));
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
        ABC_FREE_STR(pWalletInfo->szUserName);

        ABC_CLEAR_FREE(pWalletInfo, sizeof(sizeof(tABC_WalletInfo)));
    }
}

/**
 * Gets wallets for a specified account.
 *
 * This function allocates and fills in an array of wallet info structures with the information
 * associated with the wallets of the given user
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param paWalletInfo          Pointer to store the allocated array of wallet info structs
 * @param pCount                Pointer to store number of wallets in the array
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_WalletGetWallets(const char *szUserName,
                             const char *szPassword,
                             tABC_WalletInfo ***paWalletInfo,
                             unsigned int *pCount,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char **aszUUIDs = NULL;
    unsigned int nUUIDs = 0;
    tABC_WalletInfo **aWalletInfo = NULL;
    tABC_SyncKeys *pKeys = NULL;

    ABC_CHECK_RET(ABC_WalletMutexLock(pError));

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(paWalletInfo);
    *paWalletInfo = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // get the array of wallet UUIDs for this account
    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountWalletList(pKeys, &aszUUIDs, &nUUIDs, pError));

    // if we got anything
    if (nUUIDs > 0)
    {
        ABC_ALLOC(aWalletInfo, sizeof(tABC_WalletInfo *) * nUUIDs);

        for (int i = 0; i < nUUIDs; i++)
        {
            tABC_WalletInfo *pInfo = NULL;
            ABC_CHECK_RET(ABC_WalletGetInfo(szUserName, szPassword, aszUUIDs[i], &pInfo, pError));

            aWalletInfo[i] = pInfo;
        }
    }

    // assign them
    *pCount = nUUIDs;
    *paWalletInfo = aWalletInfo;
    aWalletInfo = NULL;


exit:
    ABC_UtilFreeStringArray(aszUUIDs, nUUIDs);
    ABC_WalletFreeInfoArray(aWalletInfo, nUUIDs);
    ABC_SyncFreeKeys(pKeys);

    ABC_CHECK_RET(ABC_WalletMutexUnlock(pError));
    return cc;
}

/**
 * Free the wallet info array.
 *
 * This function frees the array of wallet info structs returned from ABC_WalletGetWallets.
 *
 * @param aWalletInfo   Wallet info array to be free'd
 * @param nCount        Number of elements in the array
 */
void ABC_WalletFreeInfoArray(tABC_WalletInfo **aWalletInfo,
                             unsigned int nCount)
{
    if ((aWalletInfo != NULL) && (nCount > 0))
    {
        // go through all the elements
        for (int i = 0; i < nCount; i++)
        {
            ABC_WalletFreeInfo(aWalletInfo[i]);
        }

        ABC_FREE(aWalletInfo);
    }
}

/**
 * Gets the master key for the specified wallet
 *
 * @param pMK Pointer to store the master key
 *            (note: this is not allocated and should not be free'ed by the caller)
 */
tABC_CC ABC_WalletGetMK(const char *szUserName, const char *szPassword, const char *szUUID, tABC_U08Buf *pMK, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tWalletData *pData = NULL;

    ABC_CHECK_RET(ABC_WalletMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szUUID);
    ABC_CHECK_NULL(pMK);

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(szUserName, szPassword, szUUID, &pData, pError));

    // assign the address
    ABC_BUF_SET(*pMK, pData->MK);

exit:

    ABC_WalletMutexUnlock(NULL);
    return cc;
}

/**
 * Gets the bitcoin private seed for the specified wallet
 *
 * @param pSeed Pointer to store the bitcoin private seed
 *            (note: this is not allocated and should not be free'ed by the caller)
 */
tABC_CC ABC_WalletGetBitcoinPrivateSeed(const char *szUserName, const char *szPassword, const char *szUUID, tABC_U08Buf *pSeed, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tWalletData *pData = NULL;

    ABC_CHECK_RET(ABC_WalletMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szUUID);
    ABC_CHECK_NULL(pSeed);

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(szUserName, szPassword, szUUID, &pData, pError));

    // assign the address
    ABC_BUF_SET(*pSeed, pData->BitcoinPrivateSeed);

exit:

    ABC_WalletMutexUnlock(NULL);
    return cc;
}

tABC_CC ABC_WalletGetBitcoinPrivateSeedDisk(const char *szUserName, const char *szPassword, const char *szUUID, tABC_U08Buf *pSeed, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_AccountWalletInfo info;
    tABC_SyncKeys *pKeys = NULL;

    ABC_CHECK_RET(ABC_WalletMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szUUID);
    ABC_CHECK_NULL(pSeed);

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountWalletLoad(pKeys, szUUID, &info, pError));

    // assign the address
    ABC_BUF_DUP(*pSeed, info.BitcoinSeed);

exit:
    ABC_AccountWalletInfoFree(&info);
    ABC_WalletMutexUnlock(NULL);
    return cc;
}

/**
 * Checks if the username, password and Wallet UUID are valid.
 *
 * @param szUserName    UserName for validation
 * @param szPassword    Password for validation
 * @param szUUID        Wallet UUID
 */
tABC_CC ABC_WalletCheckCredentials(const char *szUserName,
                                   const char *szPassword,
                                   const char *szUUID,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);

    // check that this is a valid user and password
    ABC_CHECK_RET(ABC_LoginCheckCredentials(szUserName, szPassword, pError));

    // cache up the wallet (this will check that the wallet UUID is valid)
    tWalletData *pData = NULL;
    ABC_CHECK_RET(ABC_WalletCacheData(szUserName, szPassword, szUUID, &pData, pError));

exit:

    return cc;
}

/**
 * Locks the mutex
 *
 * ABC_Wallet uses the same mutex as ABC_Login so that there will be no situation in
 * which one thread is in ABC_Wallet locked on a mutex and calling a thread safe ABC_Login call
 * that is locked from another thread calling a thread safe ABC_Wallet call.
 * In other words, since they call each other, they need to share a recursive mutex.
 */
static
tABC_CC ABC_WalletMutexLock(tABC_Error *pError)
{
    return ABC_MutexLock(pError);
}

/**
 * Unlocks the mutex
 *
 */
static
tABC_CC ABC_WalletMutexUnlock(tABC_Error *pError)
{
    return ABC_MutexUnlock(pError);
}

