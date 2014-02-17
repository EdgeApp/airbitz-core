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
    unsigned int    attributes;
    int             currencyNum;
    unsigned int    numAccounts;
    char            **aszAccounts;
    tABC_U08Buf     MK;
} tWalletData;

// this holds all the of the currently cached wallets
static unsigned int gWalletsCacheCount = 0;
static tWalletData **gaWalletsCacheArray = NULL;

static tABC_CC ABC_WalletCreate(tABC_WalletCreateInfo *pInfo, tABC_Error *pError);
static tABC_CC ABC_WalletSetName(const char *szUserName, const char *szPassword, const char *szUUID, const char *szName, tABC_Error *pError);
static tABC_CC ABC_WalletSetCurrencyNum(const char *szUserName, const char *szPassword, const char *szUUID, int currencyNum, tABC_Error *pError);
static tABC_CC ABC_WalletSetAttributes(const char *szUserName, const char *szPassword, const char *szUUID, unsigned int attributes, tABC_Error *pError);
static tABC_CC ABC_WalletAddAccount(const char *szUserName, const char *szPassword, const char *szUUID, const char *szAccount, tABC_Error *pError);
static tABC_CC ABC_WalletCreateRootDir(tABC_Error *pError);
static tABC_CC ABC_WalletGetRootDirName(char **pszRootDir, tABC_Error *pError);
static tABC_CC ABC_WalletGetDirName(char **pszDir, const char *szWalletUUID, tABC_Error *pError);
static tABC_CC ABC_WalletCacheData(const char *szUserName, const char *szPassword, const char *szUUID, tWalletData **ppData, tABC_Error *pError);
static tABC_CC ABC_WalletAddToCache(tWalletData *pData, tABC_Error *pError);
static tABC_CC ABC_WalletGetFromCacheByUUID(const char *szUUID, tWalletData **ppData, tABC_Error *pError);
static void    ABC_WalletFreeData(tWalletData *pData);

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

        results.requestType = ABC_RequestType_CreateWallet;
        
        results.bSuccess = false;
        
        // create the wallet
        tABC_CC CC = ABC_WalletCreate(pInfo, &(results.errorInfo));
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

// creates the wallet with the given info
tABC_CC ABC_WalletCreate(tABC_WalletCreateInfo *pInfo,
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
static
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
    sprintf(szFilename, "%s/%s", pData->szWalletDir, WALLET_NAME_FILENAME);
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
    sprintf(szFilename, "%s/%s", pData->szWalletDir, WALLET_CURRENCY_FILENAME);
    tABC_U08Buf Data;
    ABC_BUF_SET_PTR(Data, (unsigned char *)szJSON, strlen(szJSON) + 1);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(Data, pData->MK, ABC_CryptoType_AES256, szFilename, pError));
    
exit:
    if (szFilename) free(szFilename);
    if (szJSON) free(szJSON);
    
    return cc;
}

// sets the attributes of a wallet
static
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
    sprintf(szFilename, "%s/%s", pData->szWalletDir, WALLET_ATTRIBUTES_FILENAME);
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
    sprintf(szFilename, "%s/%s", pData->szWalletDir, WALLET_ACCOUNTS_FILENAME);
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
    sprintf(*pszDir, "%s/%s", szWalletRootDir, szWalletUUID);
    
exit:
    if (szWalletRootDir) free(szWalletRootDir);
    
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
        ABC_CHECK_RET(ABC_AccountGetSyncDirName(pData->szUserName, &szAccountSyncDir, pError));
        
        // create a new wallet data struct
        pData = calloc(1, sizeof(tWalletData));
        pData->szUserName = strdup(szUserName);
        pData->szPassword = strdup(szPassword);
        pData->szUUID = strdup(szUUID);
        ABC_CHECK_RET(ABC_WalletGetDirName(&(pData->szWalletDir), szUUID, pError));
        
        // get LP2 for this account
        ABC_CHECK_RET(ABC_AccountGetKey(pData->szUserName, pData->szPassword, ABC_AccountKey_LP2, &LP2, pError));
        
        // get the MK
        szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
        sprintf(szFilename, "%s/%s%s.json", szAccountSyncDir, WALLET_EMK_PREFIX, szUUID);
        ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, LP2, &(pData->MK), pError));
        
        // get the name
        sprintf(szFilename, "%s/%s", pData->szWalletDir, WALLET_NAME_FILENAME);
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
        sprintf(szFilename, "%s/%s", pData->szWalletDir, WALLET_ATTRIBUTES_FILENAME);
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
        sprintf(szFilename, "%s/%s", pData->szWalletDir, WALLET_CURRENCY_FILENAME);
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
        sprintf(szFilename, "%s/%s", pData->szWalletDir, WALLET_ACCOUNTS_FILENAME);
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
        
        pData->attributes = 0;
        pData->currencyNum = -1;
        
        ABC_UtilFreeStringArray(pData->aszAccounts, pData->numAccounts);
        pData->aszAccounts = NULL;
        
        ABC_BUF_ZERO(pData->MK);
        ABC_BUF_FREE(pData->MK);
    }
}

