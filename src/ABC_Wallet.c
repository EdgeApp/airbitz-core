/**
 * @file
 * AirBitz Wallet functions.
 *
 * This file contains all of the functions associated with wallet creation,
 * viewing and modification.
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ABC_Wallet.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Crypto.h"
#include "ABC_Account.h"

#define WALLET_KEY_LENGTH                   AES_256_KEY_LENGTH

#define WALLET_DIR                          "Wallets"
#define WALLET_SYNC_DIR                     "sync"
#define WALLET_EMK_PREFIX                   "EMK_"
#define WALLET_ACCOUNTS_WALLETS_FILENAME    "Wallets.json"
#define WALLET_NAME_FILENAME                "Wallet_Name.json"
#define WALLET_ATTRIBUTES_FILENAME          "Attributes.json"
#define WALLET_CURRENCY_FILENAME            "Currency.json"
#define WALLET_ACCOUNTS_FILENAME            "Accounts.json"

#define JSON_WALLET_WALLETS_FIELD           "wallets"
#define JSON_WALLET_NAME_FIELD              "walletName"
#define JSON_WALLET_ATTRIBUTES_FIELD        "attributes"
#define JSON_WALLET_CURRENCY_NUM_FIELD      "num"
#define JSON_WALLET_ACCOUNTS_FIELD          "accounts"

// holds wallet data (including keys) for a given wallet
typedef struct sWalletData
{
    char            *szUUID;
    char            *szName;
    char            *szUserName;
    char            *szPassword;
    char            *szWalletDir;
    char            *szWalletSyncDir;
    unsigned int    attributes;
    int             currencyNum;
    unsigned int    numAccounts;
    char            **aszAccounts;
    tABC_U08Buf     MK;
} tWalletData;

// this holds all the of the currently cached wallets
static unsigned int gWalletsCacheCount = 0;
static tWalletData **gaWalletsCacheArray = NULL;

static tABC_CC ABC_WalletCreate(tABC_WalletCreateInfo *pInfo, char **pszUUID, tABC_Error *pError);
static tABC_CC ABC_WalletSetCurrencyNum(const char *szUserName, const char *szPassword, const char *szUUID, int currencyNum, tABC_Error *pError);
static tABC_CC ABC_WalletAddAccount(const char *szUserName, const char *szPassword, const char *szUUID, const char *szAccount, tABC_Error *pError);
static tABC_CC ABC_WalletCreateRootDir(tABC_Error *pError);
static tABC_CC ABC_WalletGetRootDirName(char **pszRootDir, tABC_Error *pError);
static tABC_CC ABC_WalletGetDirName(char **pszDir, const char *szWalletUUID, tABC_Error *pError);
static tABC_CC ABC_WalletGetSyncDirName(char **pszDir, const char *szWalletUUID, tABC_Error *pError);
static tABC_CC ABC_WalletCacheData(const char *szUserName, const char *szPassword, const char *szUUID, tWalletData **ppData, tABC_Error *pError);
static tABC_CC ABC_WalletAddToCache(tWalletData *pData, tABC_Error *pError);
static tABC_CC ABC_WalletGetFromCacheByUUID(const char *szUUID, tWalletData **ppData, tABC_Error *pError);
static void    ABC_WalletFreeData(tWalletData *pData);
static tABC_CC ABC_WalletGetUUIDs(const char *szUserName, char ***paUUIDs, unsigned int *pCount, tABC_Error *pError);
static tABC_CC ABC_WalletChangeEMKForUUID(const char *szUserName, const char *szUUID, tABC_U08Buf oldLP2, tABC_U08Buf newLP2, tABC_Error *pError);

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
    ABC_CHECK_NULL(fRequestCallback);

    tABC_WalletCreateInfo *pWalletCreateInfo = (tABC_WalletCreateInfo *) calloc(1, sizeof(tABC_WalletCreateInfo));

    pWalletCreateInfo->szUserName = strdup(szUserName);
    pWalletCreateInfo->szPassword = strdup(szPassword);
    pWalletCreateInfo->szWalletName = strdup(szWalletName);
    pWalletCreateInfo->currencyNum = currencyNum;
    pWalletCreateInfo->attributes = attributes;

    pWalletCreateInfo->fRequestCallback = fRequestCallback;

    pWalletCreateInfo->pData = pData;

    *ppWalletCreateInfo = pWalletCreateInfo;

exit:

    return cc;
}

void ABC_WalletCreateInfoFree(tABC_WalletCreateInfo *pWalletCreateInfo)
{
    if (pWalletCreateInfo)
    {
        free((void *)pWalletCreateInfo->szUserName);
        free((void *)pWalletCreateInfo->szPassword);
        free((void *)pWalletCreateInfo->szWalletName);

        free(pWalletCreateInfo);
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
static
tABC_CC ABC_WalletCreate(tABC_WalletCreateInfo *pInfo,
                         char                  **pszUUID,
                         tABC_Error            *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szAccountSyncDir = NULL;
    char *szFilename = NULL;
    char *szEMK_JSON        = NULL;
    char *szJSON = NULL;
    char *szUUID = NULL;
    json_t *pJSON_Data = NULL;
    json_t *pJSON_Wallets = NULL;
    tABC_U08Buf L2 = ABC_BUF_NULL;
    tABC_U08Buf LP2 = ABC_BUF_NULL;
    tWalletData *pData = NULL;

    ABC_CHECK_NULL(pInfo);
    ABC_CHECK_NULL(pszUUID);

    // create a new wallet data struct
    pData = calloc(1, sizeof(tWalletData));
    pData->szUserName = strdup(pInfo->szUserName);
    pData->szPassword = strdup(pInfo->szPassword);

    // get the local directory for this account
    ABC_CHECK_RET(ABC_AccountGetSyncDirName(pData->szUserName, &szAccountSyncDir, pError));

    // get L2
    ABC_CHECK_RET(ABC_AccountGetKey(pData->szUserName, pData->szPassword, ABC_AccountKey_L2, &L2, pError));

    // get LP2
    ABC_CHECK_RET(ABC_AccountGetKey(pData->szUserName, pData->szPassword, ABC_AccountKey_LP2, &LP2, pError));

    // create wallet guid
    ABC_CHECK_RET(ABC_CryptoGenUUIDString(&szUUID, pError));
    pData->szUUID = strdup(szUUID);
    *pszUUID = strdup(szUUID);
    //printf("Wallet UUID: %s\n", pData->szUUID);

    // generate the master key for this wallet - MK_<Wallet_GUID1>
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(WALLET_KEY_LENGTH, &pData->MK, pError));
    //ABC_UtilHexDumpBuf("MK", pData->MK);

    // create encrypted wallet key EMK_<Wallet_UUID1>.json ( AES256(MK_<wallet_guid>, LP2) )
    ABC_CHECK_RET(ABC_CryptoEncryptJSONString(pData->MK, LP2, ABC_CryptoType_AES256, &szEMK_JSON, pError));

    // add encrypted wallet key to the account sync directory
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s%s.json", szAccountSyncDir, WALLET_EMK_PREFIX, pData->szUUID);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szEMK_JSON, pError));

    // add wallet to account Wallets.json
    sprintf(szFilename, "%s/%s", szAccountSyncDir, WALLET_ACCOUNTS_WALLETS_FILENAME);
    ABC_CHECK_RET(ABC_FileIOReadFileObject(szFilename, &pJSON_Data, false, pError));
    pJSON_Wallets = json_object_get(pJSON_Data, JSON_WALLET_WALLETS_FIELD);
    if (pJSON_Wallets)
    {
        pJSON_Wallets = json_incref(pJSON_Wallets);
    }
    else
    {
        pJSON_Wallets = json_array();
        json_object_set(pJSON_Data, JSON_WALLET_WALLETS_FIELD, pJSON_Wallets);
    }
    ABC_CHECK_ASSERT((pJSON_Wallets && json_is_array(pJSON_Wallets)), ABC_CC_JSONError, "Error parsing JSON wallets array");
    json_array_append_new(pJSON_Wallets, json_string(pData->szUUID));
    szJSON = json_dumps(pJSON_Data, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szJSON, pError));

    // TODO: create wallet directory sync key
    // TODO: create encrypted wallet directory sync key ERepoWalletKey_<Wallet_GUID1>.json ( AES256(RepoWalletKey_<Wallet_GUID1>, L2) )
    // TODO: add encrypted wallet sync key to account directory

    // create the wallet root directory if necessary
    ABC_CHECK_RET(ABC_WalletCreateRootDir(pError));

    // create the wallet directory - <Wallet_UUID1>  <- All data in this directory encrypted with MK_<Wallet_UUID1>
    ABC_CHECK_RET(ABC_WalletGetDirName(&(pData->szWalletDir), pData->szUUID, pError));
    ABC_CHECK_RET(ABC_FileIOCreateDir(pData->szWalletDir, pError));

    // create the wallet sync dir under the main dir
    ABC_CHECK_RET(ABC_WalletGetSyncDirName(&(pData->szWalletSyncDir), pData->szUUID, pError));
    ABC_CHECK_RET(ABC_FileIOCreateDir(pData->szWalletSyncDir, pError));

    // we now have a new wallet so go ahead and cache its data
    ABC_CHECK_RET(ABC_WalletAddToCache(pData, pError));
    pData = NULL; // so we don't free what we just added to the cache

    // all the functions below assume the wallet is in the cache or can be loaded into the cache

    // set the wallet name
    ABC_CHECK_RET(ABC_WalletSetName(pInfo->szUserName, pInfo->szPassword, szUUID, pInfo->szWalletName, pError));

    // set the currency
    ABC_CHECK_RET(ABC_WalletSetCurrencyNum(pInfo->szUserName, pInfo->szPassword, szUUID, pInfo->currencyNum, pError));

    // set the wallet attributes
    ABC_CHECK_RET(ABC_WalletSetAttributes(pInfo->szUserName, pInfo->szPassword, szUUID, pInfo->attributes, pError));

    // set this account for the wallet's first account
    ABC_CHECK_RET(ABC_WalletAddAccount(pInfo->szUserName, pInfo->szPassword, szUUID, pInfo->szUserName, pError));

    // TODO: Create the Private_Seed.json - bit coin seed
    // TODO: Create the BitCoin_Keys.json - bit coin keys
    // TODO: Create sync info for Wallet directory - .git

exit:
    if (szAccountSyncDir)   free(szAccountSyncDir);
    if (szFilename)         free(szFilename);
    if (szEMK_JSON)         free(szEMK_JSON);
    if (szJSON)             free(szJSON);
    if (szUUID)             free(szUUID);
    if (pJSON_Data)         json_decref(pJSON_Data);
    if (pJSON_Wallets)      json_decref(pJSON_Wallets);
    if (pData)              ABC_WalletFreeData(pData);

    return cc;
}

// sets the name of a wallet
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
    if (pData->szName)
    {
        free(pData->szName);
    }
    pData->szName = strdup(szName);

    // create the json for the wallet name
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(szName, JSON_WALLET_NAME_FIELD, &szJSON, pError));
    //printf("name:\n%s\n", szJSON);

    // write the name out to the file
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_NAME_FILENAME);
    tABC_U08Buf Data;
    ABC_BUF_SET_PTR(Data, (unsigned char *)szJSON, strlen(szJSON) + 1);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(Data, pData->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    if (szFilename) free(szFilename);
    if (szJSON) free(szJSON);

    return cc;
}

// sets the currency number of a wallet
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
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_CURRENCY_FILENAME);
    tABC_U08Buf Data;
    ABC_BUF_SET_PTR(Data, (unsigned char *)szJSON, strlen(szJSON) + 1);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(Data, pData->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    if (szFilename) free(szFilename);
    if (szJSON) free(szJSON);

    return cc;
}

// sets the attributes of a wallet
tABC_CC ABC_WalletSetAttributes(const char *szUserName, const char *szPassword, const char *szUUID, unsigned int attributes, tABC_Error *pError)
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
    pData->attributes = attributes;

    // create the json for the attributes
    ABC_CHECK_RET(ABC_UtilCreateIntJSONString((int) attributes, JSON_WALLET_ATTRIBUTES_FIELD, &szJSON, pError));
    //printf("attributes:\n%s\n", szJSON);

    // write the name out to the file
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_ATTRIBUTES_FILENAME);
    tABC_U08Buf Data;
    ABC_BUF_SET_PTR(Data, (unsigned char *)szJSON, strlen(szJSON) + 1);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(Data, pData->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    if (szFilename) free(szFilename);
    if (szJSON) free(szJSON);

    return cc;
}

// adds the given account to the list of accounts that uses this wallet
static
tABC_CC ABC_WalletAddAccount(const char *szUserName, const char *szPassword, const char *szUUID, const char *szAccount, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tWalletData *pData = NULL;
    char *szFilename = NULL;
    char *szJSON = NULL;

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
        pData->aszAccounts = calloc(1, sizeof(char *));
    }
    pData->aszAccounts[pData->numAccounts] = strdup(szAccount);
    pData->numAccounts++;

    // create the json for the accounts
    ABC_CHECK_RET(ABC_UtilCreateArrayJSONString(pData->aszAccounts, pData->numAccounts, JSON_WALLET_ACCOUNTS_FIELD, &szJSON, pError));
    //printf("accounts:\n%s\n", szJSON);

    // write the name out to the file
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_ACCOUNTS_FILENAME);
    tABC_U08Buf Data;
    ABC_BUF_SET_PTR(Data, (unsigned char *)szJSON, strlen(szJSON) + 1);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(Data, pData->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    if (szFilename) free(szFilename);
    if (szJSON) free(szJSON);

    return cc;
}

// creates the wallet directory if needed
static
tABC_CC ABC_WalletCreateRootDir(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletRoot = NULL;

    // create the account directory string
    ABC_CHECK_RET(ABC_WalletGetRootDirName(&szWalletRoot, pError));

    // if it doesn't exist
    if (ABC_FileIOFileExist(szWalletRoot) != true)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szWalletRoot, pError));
    }

exit:
    if (szWalletRoot) free(szWalletRoot);

    return cc;
}

// gets the root directory for the wallets
// the string is allocated so it is up to the caller to free it
static
tABC_CC ABC_WalletGetRootDirName(char **pszRootDir, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(pszRootDir);

    const char *szFileIORootDir;
    ABC_CHECK_RET(ABC_FileIOGetRootDir(&szFileIORootDir, pError));

    // create the wallet directory string
    *pszRootDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszRootDir, "%s/%s", szFileIORootDir, WALLET_DIR);

exit:

    return cc;
}

// gets the directory for the given wallet UUID
// the string is allocated so it is up to the caller to free it
static
tABC_CC ABC_WalletGetDirName(char **pszDir, const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletRootDir = NULL;

    ABC_CHECK_NULL(pszDir);

    ABC_CHECK_RET(ABC_WalletGetRootDirName(&szWalletRootDir, pError));

    // create the account directory string
    *pszDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_NULL(*pszDir);
    sprintf(*pszDir, "%s/%s", szWalletRootDir, szWalletUUID);

exit:
    if (szWalletRootDir) free(szWalletRootDir);

    return cc;
}

/**
 * Gets the sync directory for the given wallet UUID.
 * @param pszDir the output directory name. The caller must free this.
 */
tABC_CC ABC_WalletGetSyncDirName(char **pszDir, const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletDir = NULL;

    ABC_CHECK_NULL(pszDir);
    ABC_CHECK_NULL(szWalletUUID);

    ABC_CHECK_RET(ABC_WalletGetDirName(&szWalletDir, szWalletUUID, pError));

    *pszDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_NULL(*pszDir);
    sprintf(*pszDir, "%s/%s", szWalletDir, WALLET_SYNC_DIR);

exit:
    if (szWalletDir) free(szWalletDir);

    return cc;
}

// Adds the wallet data to the cache
// If the wallet is not currently in the cache it is added
static
tABC_CC ABC_WalletCacheData(const char *szUserName, const char *szPassword, const char *szUUID, tWalletData **ppData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tWalletData *pData = NULL;
    char *szAccountSyncDir = NULL;
    tABC_U08Buf LP2 = ABC_BUF_NULL;
    char *szFilename = NULL;
    tABC_U08Buf Data = ABC_BUF_NULL;

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

        // get the local directory for this account
        ABC_CHECK_RET(ABC_AccountGetSyncDirName(szUserName, &szAccountSyncDir, pError));

        // create a new wallet data struct
        pData = calloc(1, sizeof(tWalletData));
        pData->szUserName = strdup(szUserName);
        pData->szPassword = strdup(szPassword);
        pData->szUUID = strdup(szUUID);
        ABC_CHECK_RET(ABC_WalletGetDirName(&(pData->szWalletDir), szUUID, pError));
        ABC_CHECK_RET(ABC_WalletGetSyncDirName(&(pData->szWalletSyncDir), szUUID, pError));

        // get LP2 for this account
        ABC_CHECK_RET(ABC_AccountGetKey(pData->szUserName, pData->szPassword, ABC_AccountKey_LP2, &LP2, pError));

        // get the MK
        szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
        sprintf(szFilename, "%s/%s%s.json", szAccountSyncDir, WALLET_EMK_PREFIX, szUUID);
        ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, LP2, &(pData->MK), pError));

        // get the name
        sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_NAME_FILENAME);
        if (true == ABC_FileIOFileExist(szFilename))
        {
            ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, pData->MK, &Data, pError));
            ABC_CHECK_RET(ABC_UtilGetStringValueFromJSONString((char *)ABC_BUF_PTR(Data), JSON_WALLET_NAME_FIELD, &(pData->szName), pError));
            ABC_BUF_FREE(Data);
        }
        else
        {
            pData->szName = strdup("");
        }

        // get the attributes
        sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_ATTRIBUTES_FILENAME);
        if (true == ABC_FileIOFileExist(szFilename))
        {
            ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, pData->MK, &Data, pError));
            ABC_CHECK_RET(ABC_UtilGetIntValueFromJSONString((char *)ABC_BUF_PTR(Data), JSON_WALLET_ATTRIBUTES_FIELD, (int *) &(pData->attributes), pError));
            ABC_BUF_FREE(Data);
        }
        else
        {
            pData->attributes = 0;
        }

        // get the currency num
        sprintf(szFilename, "%s/%s", pData->szWalletSyncDir, WALLET_CURRENCY_FILENAME);
        if (true == ABC_FileIOFileExist(szFilename))
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
        if (true == ABC_FileIOFileExist(szFilename))
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
        free(pData);
    }
    if (szAccountSyncDir) free(szAccountSyncDir);
    if (szFilename) free(szFilename);
    ABC_BUF_FREE(Data);


    return cc;
}

// clears all the data from the cache
tABC_CC ABC_WalletClearCache(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    if ((gWalletsCacheCount > 0) && (NULL != gaWalletsCacheArray))
    {
        for (int i = 0; i < gWalletsCacheCount; i++)
        {
            tWalletData *pData = gaWalletsCacheArray[i];
            ABC_WalletFreeData(pData);
        }

        free(gaWalletsCacheArray);
        gWalletsCacheCount = 0;
    }

exit:

    return cc;
}

// adds the given WalletDAta to the array of cached wallets
static
tABC_CC ABC_WalletAddToCache(tWalletData *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

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
            gaWalletsCacheArray = malloc(sizeof(tWalletData *));
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

    return cc;
}

// searches for a wallet in the cached by UUID
// if it is not found, the wallet data will be set to NULL
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

// free's the given wallet data elements
static
void ABC_WalletFreeData(tWalletData *pData)
{
    if (pData)
    {
        if (pData->szUUID)
        {
            memset(pData->szUUID, 0, strlen(pData->szUUID));
            free(pData->szUUID);
            pData->szUUID = NULL;
        }

        if (pData->szName)
        {
            memset(pData->szName, 0, strlen(pData->szName));
            free(pData->szName);
            pData->szName = NULL;
        }

        if (pData->szUserName)
        {
            memset(pData->szUserName, 0, strlen(pData->szUserName));
            free(pData->szUserName);
            pData->szUserName = NULL;
        }

        if (pData->szPassword)
        {
            memset(pData->szPassword, 0, strlen(pData->szPassword));
            free(pData->szPassword);
            pData->szPassword = NULL;
        }

        if (pData->szWalletDir)
        {
            memset(pData->szWalletDir, 0, strlen(pData->szWalletDir));
            free(pData->szWalletDir);
            pData->szWalletDir = NULL;
        }

        if (pData->szWalletSyncDir)
        {
            memset(pData->szWalletSyncDir, 0, strlen(pData->szWalletSyncDir));
            free(pData->szWalletSyncDir);
            pData->szWalletSyncDir = NULL;
        }

        pData->attributes = 0;
        pData->currencyNum = -1;

        ABC_UtilFreeStringArray(pData->aszAccounts, pData->numAccounts);
        pData->aszAccounts = NULL;

        ABC_BUF_ZERO(pData->MK);
        ABC_BUF_FREE(pData->MK);
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

    tWalletData     *pData = NULL;
    tABC_WalletInfo *pInfo = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szUUID);
    ABC_CHECK_NULL(ppWalletInfo);

    // load the wallet data into the cache
    ABC_CHECK_RET(ABC_WalletCacheData(szUserName, szPassword, szUUID, &pData, pError));

    // create the wallet info struct
    pInfo = calloc(1, sizeof(tABC_WalletInfo));

    // copy data from what was cached
    pInfo->szUUID      = strdup(szUUID);
    pInfo->szName      = (pData->szName != NULL ? strdup(pData->szName) : NULL);
    pInfo->szUserName  = (pData->szUserName != NULL ? strdup(pData->szUserName) : NULL);
    pInfo->currencyNum = pData->currencyNum;
    pInfo->attributes  = pData->attributes;

    // TODO: get the balance

    // assign it to the user's pointer
    *ppWalletInfo = pInfo;
    pInfo = NULL;

exit:
    if (pInfo) free(pInfo);

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
        free(pWalletInfo->szUUID);
        free(pWalletInfo->szName);
        free(pWalletInfo->szUserName);

        memset(pWalletInfo, 0, sizeof(sizeof(tABC_WalletInfo)));

        free(pWalletInfo);
    }
}

/**
 * Gets wallets UUIDs for a specified account.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param paWalletInfo          Pointer to store the allocated array of UUID strings
 * @param pCount                Pointer to store number of UUIDs in the array
 * @param pError                A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_WalletGetUUIDs(const char *szUserName,
                           char ***paUUIDs,
                           unsigned int *pCount,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szAccountSyncDir = NULL;
    char *szFilename = NULL;
    char *szJSON = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(paUUIDs);
    ABC_CHECK_NULL(pCount);

    // get the local directory for this account
    ABC_CHECK_RET(ABC_AccountGetSyncDirName(szUserName, &szAccountSyncDir, pError));

    // load wallet the account Wallets.json
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", szAccountSyncDir, WALLET_ACCOUNTS_WALLETS_FILENAME);
    if (ABC_FileIOFileExist(szFilename))
    {
        ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, &szJSON, pError));
        ABC_CHECK_RET(ABC_UtilGetArrayValuesFromJSONString(szJSON, JSON_WALLET_WALLETS_FIELD, paUUIDs, pCount, pError));
    }

exit:
    if (szAccountSyncDir)   free(szAccountSyncDir);
    if (szFilename)         free(szFilename);
    if (szJSON)             free(szJSON);

    return cc;
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

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(paWalletInfo);
    *paWalletInfo = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // get the array of wallet UUIDs for this account
    ABC_CHECK_RET(ABC_WalletGetUUIDs(szUserName, &aszUUIDs, &nUUIDs, pError));

    // if we got anything
    if (nUUIDs > 0)
    {
        aWalletInfo = calloc(1, sizeof(tABC_WalletInfo *) * nUUIDs);

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

        free(aWalletInfo);
    }
}

/**
 * Set the wallet order for a specified account.
 *
 * This function sets the order of the wallets for an account to the order in the given
 * array.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param aszUUIDArray          Array of UUID strings
 * @param countUUIDs            Number of UUID's in aszUUIDArray
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_WalletSetOrder(const char *szUserName,
                           const char *szPassword,
                           char **aszUUIDArray,
                           unsigned int countUUIDs,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szAccountSyncDir = NULL;
    char *szFilename = NULL;
    char *szJSON = NULL;
    char **aszCurUUIDs = NULL;
    unsigned int nCurUUIDs = -1;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(aszUUIDArray);

    // check the credentials
    ABC_CHECK_RET(ABC_AccountCheckCredentials(szUserName, szPassword, pError));

    // get the local directory for this account
    ABC_CHECK_RET(ABC_AccountGetSyncDirName(szUserName, &szAccountSyncDir, pError));

    // load wallet the account Wallets.json
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", szAccountSyncDir, WALLET_ACCOUNTS_WALLETS_FILENAME);
    if (ABC_FileIOFileExist(szFilename))
    {
        ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, &szJSON, pError));
        ABC_CHECK_RET(ABC_UtilGetArrayValuesFromJSONString(szJSON, JSON_WALLET_WALLETS_FIELD, &aszCurUUIDs, &nCurUUIDs, pError));
        free(szJSON);
        szJSON = NULL;
    }
    else
    {
        ABC_RET_ERROR(ABC_CC_Error, "No wallets");
    }

    // make sure the count matches
    ABC_CHECK_ASSERT(nCurUUIDs == countUUIDs, ABC_CC_Error, "Number of wallets does not match");

    // make sure all of the UUIDs in the current wallet are in the new list
    for (int nCur = 0; nCur < nCurUUIDs; nCur++)
    {
        char *szCurUUID = aszCurUUIDs[nCur];

        // look through the ones given for this one
        int nNew;
        for (nNew = 0; nNew < countUUIDs; nNew++)
        {
            // found it
            if (0 == strcmp(szCurUUID, aszUUIDArray[nNew]))
            {
                break;
            }
        }

        // if we didn't find it
        if (nNew >= countUUIDs)
        {
            ABC_RET_ERROR(ABC_CC_Error, "Wallet missing from new list");
        }
    }

    // create JSON for new order
    ABC_CHECK_RET(ABC_UtilCreateArrayJSONString(aszUUIDArray, countUUIDs, JSON_WALLET_WALLETS_FIELD, &szJSON, pError));

    // write out the new json
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szJSON, pError));

exit:
    if (szAccountSyncDir)   free(szAccountSyncDir);
    if (szFilename)         free(szFilename);
    if (szJSON)             free(szJSON);
    ABC_UtilFreeStringArray(aszCurUUIDs, nCurUUIDs);

    return cc;
}

/**
 * Re-encrypted the Master Keys for the wallets for a specified account.
 *
 * @param szUserName    UserName for the account associated with this wallet
 * @param oldLP2        Previous key used to encrypted the master keys
 * @param newLP2        The new key used to encrypted the master keys
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_WalletChangeEMKsForAccount(const char *szUserName,
                                       tABC_U08Buf oldLP2,
                                       tABC_U08Buf newLP2,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char **aszUUIDs = NULL;
    unsigned int nUUIDs = 0;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL_BUF(oldLP2);
    ABC_CHECK_NULL_BUF(newLP2);

    // TODO: This function must be protected by a mutex in some way as it is changing the keys that another thread might use

    // clear the cache so no old keys will be flushed
    ABC_CHECK_RET(ABC_WalletClearCache(pError));

    // get the array of wallet UUIDs for this account
    ABC_CHECK_RET(ABC_WalletGetUUIDs(szUserName, &aszUUIDs, &nUUIDs, pError));

    // if we got anything
    if (nUUIDs > 0)
    {
        for (int i = 0; i < nUUIDs; i++)
        {
            ABC_CHECK_RET(ABC_WalletChangeEMKForUUID(szUserName, aszUUIDs[i], oldLP2, newLP2, pError));
        }
    }

exit:
    ABC_UtilFreeStringArray(aszUUIDs, nUUIDs);

    return cc;
}

/**
 * Re-encrypted the Master Keys for the wallets for a specified account.
 *
 * @param szUserName    UserName for the account associated with this wallet
 * @param oldLP2        Previous key used to encrypted the master keys
 * @param newLP2        The new key used to encrypted the master keys
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_WalletChangeEMKForUUID(const char *szUserName,
                                   const char *szUUID,
                                   tABC_U08Buf oldLP2,
                                   tABC_U08Buf newLP2,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf MK = ABC_BUF_NULL;
    char *szAccountSyncDir = NULL;
    char *szFilename = NULL;
    char *szJSON = NULL;
    char **aszUUIDs = NULL;
    unsigned int nUUIDs = 0;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szUUID);
    ABC_CHECK_NULL_BUF(oldLP2);
    ABC_CHECK_NULL_BUF(newLP2);

    // get the local directory for this account
    ABC_CHECK_RET(ABC_AccountGetSyncDirName(szUserName, &szAccountSyncDir, pError));

    // get the MK using the old LP2
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s%s.json", szAccountSyncDir, WALLET_EMK_PREFIX, szUUID);
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, oldLP2, &MK, pError));

    // write it back out using the new LP2
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(MK, newLP2, ABC_CryptoType_AES256, szFilename, pError));

exit:
    ABC_BUF_FREE(MK);
    if (szAccountSyncDir)   free(szAccountSyncDir);
    if (szFilename)         free(szFilename);
    if (szJSON)             free(szJSON);
    ABC_UtilFreeStringArray(aszUUIDs, nUUIDs);

    return cc;
}

