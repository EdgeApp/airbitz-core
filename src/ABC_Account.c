/**
 * @file
 * AirBitz Account functions.
 *
 * This file contains all of the functions associated with account creation, 
 * viewing and modification.
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <jansson.h>
#include "ABC_Account.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Crypto.h"

#define ACCOUNT_MAX                     1024  // maximum number of accounts
#define ACCOUNT_DIR                     "Accounts"
#define ACCOUNT_SYNC_DIR                "sync"
#define ACCOUNT_FOLDER_PREFIX           "Account_"
#define ACCOUNT_NAME_FILENAME           "User_Name.json"
#define ACCOUNT_EPIN_FILENAME           "EPIN.json"
#define ACCOUNT_CARE_PACKAGE_FILENAME   "Care_Package.json"
#define ACCOUNT_WALLETS_FILENAME        "Wallets.json"
#define ACCOUNT_CATEGORIES_FILENAME     "Categories.json"
#define ACCOUNT_ELP2_FILENAME           "ELP2.json"
#define ACCOUNT_ELRA2_FILENAME          "ELRA2.json"

#define JSON_ACCT_USERNAME_FIELD        "userName"
#define JSON_ACCT_PIN_FIELD             "PIN"
#define JSON_ACCT_QUESTIONS_FIELD       "questions"
#define JSON_ACCT_WALLETS_FIELD         "wallets"
#define JSON_ACCT_CATEGORIES_FIELD      "categories"
#define JSON_ACCT_ERQ_FIELD             "ERQ"
#define JSON_ACCT_SNRP_FIELD_PREFIX     "SNRP"
#define JSON_ACCT_L1_FIELD              "L1"
#define JSON_ACCT_P1_FIELD              "P1"
#define JSON_ACCT_LRA1_FIELD            "LRA1"

// holds keys for a given account
typedef struct sAccountKeys
{
    int             accountNum; // this is the number in the account directory - Account_x
    char            *szUserName;
    char            *szPassword;
    char            *szPIN;
    tABC_CryptoSNRP *pSNRP1;
    tABC_CryptoSNRP *pSNRP2;
    tABC_CryptoSNRP *pSNRP3;
    tABC_CryptoSNRP *pSNRP4;
    tABC_U08Buf     L;
    tABC_U08Buf     L1;
    tABC_U08Buf     P;
    tABC_U08Buf     P1;
    tABC_U08Buf     LRA;
    tABC_U08Buf     LRA1;
    tABC_U08Buf     L2;
    tABC_U08Buf     RQ;
    tABC_U08Buf     LP;
    tABC_U08Buf     LP2;
    tABC_U08Buf     LRA2;
} tAccountKeys;

// this holds all the of the currently cached account keys
static unsigned int gAccountKeysCacheCount = 0;
static tAccountKeys **gaAccountKeysCacheArray = NULL;


static tABC_CC ABC_AccountSignIn(tABC_AccountSignInInfo *pInfo, tABC_Error *pError);
static tABC_CC ABC_AccountCreate(tABC_AccountCreateInfo *pInfo, tABC_Error *pError);
static tABC_CC ABC_AccountSetRecovery(tABC_AccountSetRecoveryInfo *pInfo, tABC_Error *pError);
static tABC_CC ABC_AccountCreateCarePackageJSONString(const json_t *pJSON_ERQ, const json_t *pJSON_SNRP2, const json_t *pJSON_SNRP3, const json_t *pJSON_SNRP4, char **pszJSON, tABC_Error *pError);
static tABC_CC ABC_AccountGetCarePackageObjects(int AccountNum, json_t **ppJSON_ERQ, json_t **ppJSON_SNRP2, json_t **ppJSON_SNRP3, json_t **ppJSON_SNRP4, tABC_Error *pError);
static tABC_CC ABC_AccountCreateSync(const char *szAccountsRootDir, tABC_Error *pError);
static tABC_CC ABC_AccountNextAccountNum(int *pAccountNum, tABC_Error *pError);
static tABC_CC ABC_AccountCreateRootDir(tABC_Error *pError);
static tABC_CC ABC_AccountCopyRootDirName(char *szRootDir, tABC_Error *pError);
static tABC_CC ABC_AccountCopyAccountDirName(char *szAccountDir, int AccountNum, tABC_Error *pError);
static tABC_CC ABC_AccountCreateListJSON(const char *szName, const char *szItems, char **pszJSON,  tABC_Error *pError);
static tABC_CC ABC_AccountNumForUser(const char *szUserName, int *pAccountNum, tABC_Error *pError);
static tABC_CC ABC_AccountUserForNum(unsigned int AccountNum, char **pszUserName, tABC_Error *pError);
static void    ABC_AccountFreeAccountKeys(tAccountKeys *pAccountKeys);
static tABC_CC ABC_AccountAddToKeyCache(tAccountKeys *pAccountKeys, tABC_Error *pError);
static tABC_CC ABC_AccountKeyFromCacheByName(const char *szUserName, tAccountKeys **ppAccountKeys, tABC_Error *pError);
static tABC_CC ABC_AccountCacheKeys(const char *szUserName, const char *szPassword, tAccountKeys **ppKeys, tABC_Error *pError);
static tABC_CC ABC_AccountSaveCategories(const char *szUserName, char **aszCategories, unsigned int Count, tABC_Error *pError);

// allocates the account creation info structure
tABC_CC ABC_AccountCreateInfoAlloc(tABC_AccountCreateInfo **ppAccountCreateInfo,
                                   const char *szUserName,
                                   const char *szPassword,
                                   const char *szPIN,
                                   tABC_Request_Callback fRequestCallback,
                                   void *pData,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(ppAccountCreateInfo);
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szPIN);
    ABC_CHECK_NULL(fRequestCallback);

    tABC_AccountCreateInfo *pAccountCreateInfo = (tABC_AccountCreateInfo *) calloc(1, sizeof(tABC_AccountCreateInfo));
    
    pAccountCreateInfo->szUserName = strdup(szUserName);
    pAccountCreateInfo->szPassword = strdup(szPassword);
    pAccountCreateInfo->szPIN = strdup(szPIN);
    
    pAccountCreateInfo->fRequestCallback = fRequestCallback;
    
    pAccountCreateInfo->pData = pData;
    
    *ppAccountCreateInfo = pAccountCreateInfo;
    
exit:

    return cc;
}

// frees the account creation info structure
void ABC_AccountCreateInfoFree(tABC_AccountCreateInfo *pAccountCreateInfo)
{
    if (pAccountCreateInfo)
    {
        free((void *)pAccountCreateInfo->szUserName);
        free((void *)pAccountCreateInfo->szPassword);
        free((void *)pAccountCreateInfo->szPIN);
        
        free(pAccountCreateInfo);
    }
}

// allocates the account signin info structure
tABC_CC ABC_AccountSignInInfoAlloc(tABC_AccountSignInInfo **ppAccountSignInInfo,
                                   const char *szUserName,
                                   const char *szPassword,
                                   tABC_Request_Callback fRequestCallback,
                                   void *pData,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    ABC_CHECK_NULL(ppAccountSignInInfo);
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(fRequestCallback);
    
    tABC_AccountSignInInfo *pAccountSignInInfo = (tABC_AccountSignInInfo *) calloc(1, sizeof(tABC_AccountSignInInfo));
    
    pAccountSignInInfo->szUserName = strdup(szUserName);
    pAccountSignInInfo->szPassword = strdup(szPassword);
    
    pAccountSignInInfo->fRequestCallback = fRequestCallback;
    
    pAccountSignInInfo->pData = pData;
    
    *ppAccountSignInInfo = pAccountSignInInfo;
    
exit:
    
    return cc;
}

// frees the account signin info structure
void ABC_AccountSignInInfoFree(tABC_AccountSignInInfo *pAccountSignInInfo)
{
    if (pAccountSignInInfo)
    {
        free((void *)pAccountSignInInfo->szUserName);
        free((void *)pAccountSignInInfo->szPassword);
        
        free(pAccountSignInInfo);
    }
}

// allocates the account set recovery questions info structure
tABC_CC ABC_AccountSetRecoveryInfoAlloc(tABC_AccountSetRecoveryInfo **ppAccountSetRecoveryInfo,
                                        const char *szUserName,
                                        const char *szPassword,
                                        const char *szRecoveryQuestions,
                                        const char *szRecoveryAnswers,
                                        tABC_Request_Callback fRequestCallback,
                                        void *pData,
                                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(ppAccountSetRecoveryInfo);
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szRecoveryQuestions);
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_NULL(fRequestCallback);

    tABC_AccountSetRecoveryInfo *pAccountSetRecoveryInfo = (tABC_AccountSetRecoveryInfo *) calloc(1, sizeof(tABC_AccountSetRecoveryInfo));
    
    pAccountSetRecoveryInfo->szUserName = strdup(szUserName);
    pAccountSetRecoveryInfo->szPassword = strdup(szPassword);
    pAccountSetRecoveryInfo->szRecoveryQuestions = strdup(szRecoveryQuestions);
    pAccountSetRecoveryInfo->szRecoveryAnswers = strdup(szRecoveryAnswers);
    
    pAccountSetRecoveryInfo->fRequestCallback = fRequestCallback;
    
    pAccountSetRecoveryInfo->pData = pData;
    
    *ppAccountSetRecoveryInfo = pAccountSetRecoveryInfo;
    
exit:

    return cc;
}

// frees the account set recovery questions info structure
void ABC_AccountSetRecoveryInfoFree(tABC_AccountSetRecoveryInfo *pAccountSetRecoveryInfo)
{
    if (pAccountSetRecoveryInfo)
    {
        free((void *)pAccountSetRecoveryInfo->szUserName);
        free((void *)pAccountSetRecoveryInfo->szPassword);
        free((void *)pAccountSetRecoveryInfo->szRecoveryQuestions);
        free((void *)pAccountSetRecoveryInfo->szRecoveryAnswers);
        
        free(pAccountSetRecoveryInfo);
    }
}

/**
 * SignIn to an account. Assumes it is running in a thread.
 *
 * This function signs into an account.
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to create an account (should be a tABC_AccountSignInInfo)
 */
void *ABC_AccountSignInThreaded(void *pData)
{
    tABC_AccountSignInInfo *pInfo = (tABC_AccountSignInInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;
        
        results.requestType = ABC_RequestType_AccountSignIn;
        
        results.bSuccess = false;
        
        // create the account
        tABC_CC CC = ABC_AccountSignIn(pInfo, &(results.errorInfo));
        results.errorInfo.code = CC;
        
        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);
        
        // it is our responsibility to free the info struct
        ABC_AccountSignInInfoFree(pInfo);
    }
    
    return NULL;
}

/**
 * Create a new account. Assumes it is running in a thread.
 *
 * This function creates a new account. 
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to create an account (should be a tABC_AccountCreateInfo)
 */
void *ABC_AccountCreateThreaded(void *pData)
{
    tABC_AccountCreateInfo *pInfo = (tABC_AccountCreateInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;

        results.requestType = ABC_RequestType_CreateAccount;
        
        results.bSuccess = false;
        
        // create the account
        tABC_CC CC = ABC_AccountCreate(pInfo, &(results.errorInfo));
        results.errorInfo.code = CC;
        
        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);
        
        // it is our responsibility to free the info struct
        ABC_AccountCreateInfoFree(pInfo);
    }
    
    return NULL;
}

/**
 * Sets the recovery questions for an account. Assumes it is running in a thread.
 *
 * This function sets the recoveyr questions for an account. 
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to create an account (should be a tABC_AccountSetRecoveryInfo)
 */
void *ABC_AccountSetRecoveryThreaded(void *pData)
{
    tABC_AccountSetRecoveryInfo *pInfo = (tABC_AccountSetRecoveryInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;

        results.requestType = ABC_RequestType_SetAccountRecoveryQuestions;
        
        results.bSuccess = false;
        
        // create the account
        tABC_CC CC = ABC_AccountSetRecovery(pInfo, &(results.errorInfo));
        results.errorInfo.code = CC;
        
        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);
        
        // it is our responsibility to free the info struct
        ABC_AccountSetRecoveryInfoFree(pInfo);
    }
    
    return NULL;
}


// sign in to an account
// this cache's the keys for an account
static
tABC_CC ABC_AccountSignIn(tABC_AccountSignInInfo *pInfo,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    tAccountKeys    *pKeys              = NULL;
    
    ABC_CHECK_NULL(pInfo);
    
    int AccountNum = 0;
    
    // check locally for the account
    ABC_CHECK_RET(ABC_AccountNumForUser(pInfo->szUserName, &AccountNum, pError));
    if (AccountNum < 0)
    {
        ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "No account by that name");
    }
    
    // cache up the keys
    ABC_CHECK_RET(ABC_AccountCacheKeys(pInfo->szUserName, pInfo->szPassword, &pKeys, pError));
    
exit:
    
    return cc;
}

// create and account
static
tABC_CC ABC_AccountCreate(tABC_AccountCreateInfo *pInfo,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tAccountKeys    *pKeys              = NULL;
    json_t          *pJSON_SNRP2        = NULL;
    json_t          *pJSON_SNRP3        = NULL;
    json_t          *pJSON_SNRP4        = NULL;
    char            *szL1_JSON          = NULL;
    char            *szP1_JSON          = NULL;
    char            *szCarePackage_JSON = NULL;
    char            *szEPIN_JSON        = NULL;
    char            *szJSON             = NULL;
    char            *szAccountDir       = NULL;
    char            *szFilename         = NULL;

    ABC_CHECK_NULL(pInfo);

    int AccountNum = 0;
    
    // check locally that the account name is available
    ABC_CHECK_RET(ABC_AccountNumForUser(pInfo->szUserName, &AccountNum, pError));
    if (AccountNum >= 0)
    {
        ABC_RET_ERROR(ABC_CC_AccountAlreadyExists, "Account already exists");
    }

    // create an account keys struct
    pKeys = calloc(1, sizeof(tAccountKeys));
    pKeys->szUserName = strdup(pInfo->szUserName);
    pKeys->szPassword = strdup(pInfo->szPassword);
    pKeys->szPIN = strdup(pInfo->szPIN);

    // generate the SNRP's
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForServer(&(pKeys->pSNRP1), pError));
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&(pKeys->pSNRP2), pError));
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&(pKeys->pSNRP3), pError));
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&(pKeys->pSNRP4), pError));
    ABC_CHECK_RET(ABC_CryptoCreateJSONObjectSNRP(pKeys->pSNRP2, &pJSON_SNRP2, pError));
    ABC_CHECK_RET(ABC_CryptoCreateJSONObjectSNRP(pKeys->pSNRP3, &pJSON_SNRP3, pError));
    ABC_CHECK_RET(ABC_CryptoCreateJSONObjectSNRP(pKeys->pSNRP4, &pJSON_SNRP4, pError));

    // L = username
    ABC_BUF_DUP_PTR(pKeys->L, pKeys->szUserName, strlen(pKeys->szUserName));
    //ABC_UtilHexDumpBuf("L", pKeys->L);

    // L1 = Scrypt(L, SNRP1)
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->L, pKeys->pSNRP1, &(pKeys->L1), pError));
    ABC_CHECK_RET(ABC_UtilCreateHexDataJSONString(pKeys->L1, JSON_ACCT_L1_FIELD, &szL1_JSON, pError));

    // P = password
    ABC_BUF_DUP_PTR(pKeys->P, pKeys->szPassword, strlen(pKeys->szPassword));
    //ABC_UtilHexDumpBuf("P", pKeys->P);

    // P1 = Scrypt(P, SNRP1)
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->P, pKeys->pSNRP1, &(pKeys->P1), pError));
    ABC_CHECK_RET(ABC_UtilCreateHexDataJSONString(pKeys->P1, JSON_ACCT_P1_FIELD, &szP1_JSON, pError));
    
    // CarePackage = ERQ, SNRP2, SNRP3, SNRP4
    ABC_CHECK_RET(ABC_AccountCreateCarePackageJSONString(NULL, pJSON_SNRP2, pJSON_SNRP3, pJSON_SNRP4, &szCarePackage_JSON, pError));

    // TODO: create RepoAcctKey and ERepoAcctKey

    // TODO: check with the server that the account name is available while also sending the data it will need
    // Client sends L1, P1, RepoAcctKey and ERepoAcctKey to the server
    // szL1_JSON, szP1_JSON

    // create client side data

    // LP = L + P
    ABC_BUF_DUP(pKeys->LP, pKeys->L);
    ABC_BUF_APPEND(pKeys->LP, pKeys->P);
    //ABC_UtilHexDumpBuf("LP", pKeys->LP);

    // LP2 = Scrypt(L + P, SNRP2)
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->LP, pKeys->pSNRP2, &(pKeys->LP2), pError));
    
    // find the next available account number on this device
    ABC_CHECK_RET(ABC_AccountNextAccountNum(&(pKeys->accountNum), pError));

    // create the main account directory
    szAccountDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, pKeys->accountNum, pError));
    ABC_CHECK_RET(ABC_FileIOCreateDir(szAccountDir, pError));

    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    
    // create the name file data and write the file
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(pKeys->szUserName, JSON_ACCT_USERNAME_FIELD, &szJSON, pError));
    sprintf(szFilename, "%s/%s", szAccountDir, ACCOUNT_NAME_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szJSON, pError));
    free(szJSON);
    szJSON = NULL;

    // create the PIN JSON
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(pKeys->szPIN, JSON_ACCT_PIN_FIELD, &szJSON, pError));
    tABC_U08Buf PIN = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(PIN, (unsigned char *)szJSON, strlen(szJSON) + 1);
    
    // EPIN = AES256(PIN, LP2)
    ABC_CHECK_RET(ABC_CryptoEncryptJSONString(PIN, pKeys->LP2, ABC_CryptoType_AES256, &szEPIN_JSON, pError));
    sprintf(szFilename, "%s/%s", szAccountDir, ACCOUNT_EPIN_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szEPIN_JSON, pError));
    free(szJSON);
    szJSON = NULL;

    // write the file care package to a file
    sprintf(szFilename, "%s/%s", szAccountDir, ACCOUNT_CARE_PACKAGE_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szCarePackage_JSON, pError));
    
    // create the sync dir - TODO: write the sync keys to the sync dir
    ABC_CHECK_RET(ABC_AccountCreateSync(szAccountDir, pError));
    
    // we now have a new account so go ahead and cache it's keys
    ABC_CHECK_RET(ABC_AccountAddToKeyCache(pKeys, pError));
    pKeys = NULL; // so we don't free what we just added to the cache

exit:
    if (pKeys)              
    {
        ABC_AccountFreeAccountKeys(pKeys);
        free(pKeys);
    }
    if (pJSON_SNRP2)        json_decref(pJSON_SNRP2);
    if (pJSON_SNRP3)        json_decref(pJSON_SNRP3);
    if (pJSON_SNRP4)        json_decref(pJSON_SNRP4);
    if (szL1_JSON)          free(szL1_JSON);
    if (szP1_JSON)          free(szP1_JSON);
    if (szCarePackage_JSON) free(szCarePackage_JSON);
    if (szEPIN_JSON)        free(szEPIN_JSON);
    if (szJSON)             free(szJSON);
    if (szAccountDir)       free(szAccountDir);
    if (szFilename)         free(szFilename);

    return cc;
}

// set the recovery for an account
static
tABC_CC ABC_AccountSetRecovery(tABC_AccountSetRecoveryInfo *pInfo,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tAccountKeys    *pKeys              = NULL;
    json_t          *pJSON_ERQ          = NULL;
    json_t          *pJSON_SNRP2        = NULL;
    json_t          *pJSON_SNRP3        = NULL;
    json_t          *pJSON_SNRP4        = NULL;
    char            *szL1_JSON          = NULL;
    char            *szP1_JSON          = NULL;
    char            *szLRA1_JSON        = NULL;
    char            *szCarePackage_JSON = NULL;
    char            *szELP2_JSON        = NULL;
    char            *szELRA2_JSON       = NULL;
    char            *szAccountDir       = NULL;
    char            *szFilename         = NULL;

    ABC_CHECK_NULL(pInfo);

    int AccountNum = 0;
    
    // check locally for the account
    ABC_CHECK_RET(ABC_AccountNumForUser(pInfo->szUserName, &AccountNum, pError));
    if (AccountNum < 0)
    {
        ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "No account by that name");
    }
    
    // cache up the keys
    ABC_CHECK_RET(ABC_AccountCacheKeys(pInfo->szUserName, pInfo->szPassword, &pKeys, pError));
    
    // the following should all be available
    // szUserName, szPassword, szPIN, L, P, LP2, SNRP2, SNRP3, SNRP4
    ABC_CHECK_ASSERT(NULL != ABC_BUF_PTR(pKeys->L), ABC_CC_Error, "Expected to find L in key cache");
    ABC_CHECK_ASSERT(NULL != ABC_BUF_PTR(pKeys->P), ABC_CC_Error, "Expected to find P in key cache");
    ABC_CHECK_ASSERT(NULL != ABC_BUF_PTR(pKeys->LP2), ABC_CC_Error, "Expected to find LP2 in key cache");
    ABC_CHECK_ASSERT(NULL != pKeys->pSNRP3, ABC_CC_Error, "Expected to find SNRP3 in key cache");
    ABC_CHECK_ASSERT(NULL != pKeys->pSNRP4, ABC_CC_Error, "Expected to find SNRP4 in key cache");
    
    // Create the keys that we still need or that need to be updated
    
    // SNRP1
    if (NULL == pKeys->pSNRP1)
    {
        ABC_CHECK_RET(ABC_CryptoCreateSNRPForServer(&(pKeys->pSNRP1), pError));
    }
    
    // LRA = L + RA
    if (ABC_BUF_PTR(pKeys->LRA) != NULL)
    {
        ABC_BUF_FREE(pKeys->LRA);
    }
    ABC_BUF_DUP(pKeys->LRA, pKeys->L);
    ABC_BUF_APPEND_PTR(pKeys->LRA, pInfo->szRecoveryAnswers, strlen(pInfo->szRecoveryAnswers));
    //ABC_UtilHexDumpBuf("LRA", pKeys->LRA);
    
    // LRA1 = Scrypt(L + RA, SNRP1)
    if (ABC_BUF_PTR(pKeys->LRA1) != NULL)
    {
        ABC_BUF_FREE(pKeys->LRA1);
    }
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->LRA, pKeys->pSNRP1, &(pKeys->LRA1), pError));

    
    // LRA2 = Scrypt(L + RA, SNRP3)
    if (ABC_BUF_PTR(pKeys->LRA2) != NULL)
    {
        ABC_BUF_FREE(pKeys->LRA2);
    }
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->LRA, pKeys->pSNRP3, &(pKeys->LRA2), pError));
    
    // L2 = Scrypt(L, SNRP4)
    if (ABC_BUF_PTR(pKeys->L2) == NULL)
    {
        ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->L, pKeys->pSNRP4, &(pKeys->L2), pError));
    }
    
    // RQ
    if (ABC_BUF_PTR(pKeys->RQ) != NULL)
    {
        ABC_BUF_FREE(pKeys->RQ);
    }
    ABC_BUF_DUP_PTR(pKeys->RQ, pInfo->szRecoveryQuestions, strlen(pInfo->szRecoveryQuestions));
    //ABC_UtilHexDumpBuf("RQ", pKeys->RQ);
    
    // L1 = Scrypt(L, SNRP1)
    if (ABC_BUF_PTR(pKeys->L1) == NULL)
    {
        ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->L, pKeys->pSNRP1, &(pKeys->L1), pError));
    }

    // P1 = Scrypt(P, SNRP1)
    if (ABC_BUF_PTR(pKeys->P1) == NULL)
    {
        ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->P, pKeys->pSNRP1, &(pKeys->P1), pError));
    }

    // create the json objects and strings we need
    
    // ERQ = AES256(RQ, L2)
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pKeys->RQ, pKeys->L2, ABC_CryptoType_AES256, &pJSON_ERQ, pError));
    
    // ELP2 = AES256(LP2, LRA2)
    ABC_CHECK_RET(ABC_CryptoEncryptJSONString(pKeys->LP2, pKeys->LRA2, ABC_CryptoType_AES256, &szELP2_JSON, pError));
    
    // ELRA2 = AES256(LRA2, LP2)
    ABC_CHECK_RET(ABC_CryptoEncryptJSONString(pKeys->LRA2, pKeys->LP2, ABC_CryptoType_AES256, &szELRA2_JSON, pError));
    
    
    // write out the files
    
    // create the main account directory
    szAccountDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, pKeys->accountNum, pError));
    
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    
    // write ELP2.json <- LP2 (L+P,S2) encrypted with recovery key (LRA2)
    sprintf(szFilename, "%s/%s/%s", szAccountDir, ACCOUNT_SYNC_DIR, ACCOUNT_ELP2_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szELP2_JSON, pError));
    
    // write ELRA2.json <- LRA2 encrypted with LP2 (L+P,S2)
    sprintf(szFilename, "%s/%s/%s", szAccountDir, ACCOUNT_SYNC_DIR, ACCOUNT_ELRA2_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szELRA2_JSON, pError));

    
    // update the care package
    
    // get the current care package
    ABC_CHECK_RET(ABC_AccountGetCarePackageObjects(AccountNum, NULL, &pJSON_SNRP2, &pJSON_SNRP3, &pJSON_SNRP4, pError));
    
    // Create an updated CarePackage = ERQ, SNRP2, SNRP3, SNRP4
    ABC_CHECK_RET(ABC_AccountCreateCarePackageJSONString(pJSON_ERQ, pJSON_SNRP2, pJSON_SNRP3, pJSON_SNRP4, &szCarePackage_JSON, pError));
    
    // write the file care package to a file
    sprintf(szFilename, "%s/%s", szAccountDir, ACCOUNT_CARE_PACKAGE_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szCarePackage_JSON, pError));
    
    
    // TODO: Client sends L1, P1, LRA1, CarePackage, to the server
    // szL1_JSON, szP1_JSON, szLRA1_JSON, szCarePackage_JSON
    ABC_CHECK_RET(ABC_UtilCreateHexDataJSONString(pKeys->L1, JSON_ACCT_L1_FIELD, &szL1_JSON, pError));
    ABC_CHECK_RET(ABC_UtilCreateHexDataJSONString(pKeys->P1, JSON_ACCT_P1_FIELD, &szP1_JSON, pError));
    ABC_CHECK_RET(ABC_UtilCreateHexDataJSONString(pKeys->LRA1, JSON_ACCT_LRA1_FIELD, &szLRA1_JSON, pError));
    
exit:
    if (pJSON_ERQ)          json_decref(pJSON_ERQ);
    if (pJSON_SNRP2)        json_decref(pJSON_SNRP2);
    if (pJSON_SNRP3)        json_decref(pJSON_SNRP3);
    if (pJSON_SNRP4)        json_decref(pJSON_SNRP4);
    if (szL1_JSON)          free(szL1_JSON);
    if (szP1_JSON)          free(szP1_JSON);
    if (szLRA1_JSON)        free(szLRA1_JSON);
    if (szCarePackage_JSON) free(szCarePackage_JSON);
    if (szELP2_JSON)        free(szELP2_JSON);
    if (szELRA2_JSON)       free(szELRA2_JSON);
    if (szAccountDir)       free(szAccountDir);
    if (szFilename)         free(szFilename);

    return cc;
}


// creates the json care package
// pJSON_ERQ can be null
static
tABC_CC ABC_AccountCreateCarePackageJSONString(const json_t *pJSON_ERQ,
                                               const json_t *pJSON_SNRP2,
                                               const json_t *pJSON_SNRP3,
                                               const json_t *pJSON_SNRP4,
                                               char         **pszJSON, 
                                               tABC_Error   *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Root = NULL;
    char *szField = NULL;

    ABC_CHECK_NULL(pJSON_SNRP2);
    ABC_CHECK_NULL(pJSON_SNRP3);
    ABC_CHECK_NULL(pJSON_SNRP4);
    ABC_CHECK_NULL(pszJSON);

    pJSON_Root = json_object();

    if (pJSON_ERQ != NULL)
    {
        json_object_set(pJSON_Root, JSON_ACCT_ERQ_FIELD, (json_t *) pJSON_ERQ);
    }

    szField = calloc(1, ABC_MAX_STRING_LENGTH);

    sprintf(szField, "%s%d", JSON_ACCT_SNRP_FIELD_PREFIX, 2);
    json_object_set(pJSON_Root, szField, (json_t *) pJSON_SNRP2);

    sprintf(szField, "%s%d", JSON_ACCT_SNRP_FIELD_PREFIX, 3);
    json_object_set(pJSON_Root, szField, (json_t *) pJSON_SNRP3);

    sprintf(szField, "%s%d", JSON_ACCT_SNRP_FIELD_PREFIX, 4);
    json_object_set(pJSON_Root, szField, (json_t *) pJSON_SNRP4);

    *pszJSON = json_dumps(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    if (szField)    free(szField);

    return cc;
}

// loads the json care package for a given account number
// if the ERQ doesn't exist, ppJSON_ERQ is set to NULL
static
tABC_CC ABC_AccountGetCarePackageObjects(int          AccountNum,
                                         json_t       **ppJSON_ERQ,
                                         json_t       **ppJSON_SNRP2,
                                         json_t       **ppJSON_SNRP3,
                                         json_t       **ppJSON_SNRP4,
                                         tABC_Error   *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szAccountDir = NULL;
    char *szCarePackageFilename = NULL;
    char *szCarePackage_JSON = NULL;
    char *szField = NULL;
    json_t *pJSON_Root = NULL;
    json_t *pJSON_ERQ = NULL;
    json_t *pJSON_SNRP2 = NULL;
    json_t *pJSON_SNRP3 = NULL;
    json_t *pJSON_SNRP4 = NULL;

    ABC_CHECK_ASSERT(AccountNum >= 0, ABC_CC_AccountDoesNotExist, "Bad account number");
    ABC_CHECK_NULL(ppJSON_SNRP2);
    ABC_CHECK_NULL(ppJSON_SNRP3);
    ABC_CHECK_NULL(ppJSON_SNRP4);

    // get the main account directory
    szAccountDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, AccountNum, pError));
    
    // create the name of the care package file
    szCarePackageFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szCarePackageFilename, "%s/%s", szAccountDir, ACCOUNT_CARE_PACKAGE_FILENAME);

    // load the care package
    ABC_CHECK_RET(ABC_FileIOReadFileStr(szCarePackageFilename, &szCarePackage_JSON, pError));

    // decode the json
    json_error_t error;
    pJSON_Root = json_loads(szCarePackage_JSON, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON care package");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON care package");
   
    // get the ERQ
    if (ppJSON_ERQ)
    {
        pJSON_ERQ = json_object_get(pJSON_Root, JSON_ACCT_ERQ_FIELD);
        //ABC_CHECK_ASSERT((pJSON_ERQ && json_is_object(pJSON_ERQ)), ABC_CC_JSONError, "Error parsing JSON care package - missing ERQ");
    }

    szField = calloc(1, ABC_MAX_STRING_LENGTH);

    // get SNRP2
    sprintf(szField, "%s%d", JSON_ACCT_SNRP_FIELD_PREFIX, 2);
    pJSON_SNRP2 = json_object_get(pJSON_Root, szField);
    ABC_CHECK_ASSERT((pJSON_SNRP2 && json_is_object(pJSON_SNRP2)), ABC_CC_JSONError, "Error parsing JSON care package - missing SNRP2");

    // get SNRP3
    sprintf(szField, "%s%d", JSON_ACCT_SNRP_FIELD_PREFIX, 3);
    pJSON_SNRP3 = json_object_get(pJSON_Root, szField);
    ABC_CHECK_ASSERT((pJSON_SNRP3 && json_is_object(pJSON_SNRP3)), ABC_CC_JSONError, "Error parsing JSON care package - missing SNRP3");

    // get SNRP4
    sprintf(szField, "%s%d", JSON_ACCT_SNRP_FIELD_PREFIX, 4);
    pJSON_SNRP4 = json_object_get(pJSON_Root, szField);
    ABC_CHECK_ASSERT((pJSON_SNRP4 && json_is_object(pJSON_SNRP4)), ABC_CC_JSONError, "Error parsing JSON care package - missing SNRP4");

    // assign what we found (we need to increment the refs because they were borrowed from root)
    if (ppJSON_ERQ)
    {
        if (pJSON_ERQ)
        {
            *ppJSON_ERQ = json_incref(pJSON_ERQ);
            pJSON_ERQ = NULL; // don't decrment this at the end
        }
        else
        {
            *ppJSON_ERQ = NULL;
        }
    }
    *ppJSON_SNRP2 = json_incref(pJSON_SNRP2);
    pJSON_SNRP2 = NULL; // don't decrment this at the end
    *ppJSON_SNRP3 = json_incref(pJSON_SNRP3);
    pJSON_SNRP3 = NULL; // don't decrment this at the end
    *ppJSON_SNRP4 = json_incref(pJSON_SNRP4);
    pJSON_SNRP4 = NULL; // don't decrment this at the end

exit:
    if (pJSON_Root)             json_decref(pJSON_Root);
    if (pJSON_ERQ)              json_decref(pJSON_ERQ);
    if (pJSON_SNRP2)            json_decref(pJSON_SNRP2);
    if (pJSON_SNRP3)            json_decref(pJSON_SNRP3);
    if (pJSON_SNRP4)            json_decref(pJSON_SNRP4);
    if (szAccountDir)           free(szAccountDir);
    if (szCarePackageFilename)  free(szCarePackageFilename);
    if (szCarePackage_JSON)     free(szCarePackage_JSON);
    if (szField)                free(szField);

    return cc;
}

// creates a new sync directory and all the files needed for the given account
// TODO: eventually this function needs the sync info
static
tABC_CC ABC_AccountCreateSync(const char *szAccountsRootDir,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szDataJSON = NULL;
    char *szFilename = NULL;

    ABC_CHECK_NULL(szAccountsRootDir);
    
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    
    // create the sync directory
    sprintf(szFilename, "%s/%s", szAccountsRootDir, ACCOUNT_SYNC_DIR);
    ABC_CHECK_RET(ABC_FileIOCreateDir(szFilename, pError));

    // create initial categories file with no entries
    ABC_CHECK_RET(ABC_AccountCreateListJSON(JSON_ACCT_CATEGORIES_FIELD, "", &szDataJSON, pError));
    sprintf(szFilename, "%s/%s/%s", szAccountsRootDir, ACCOUNT_SYNC_DIR, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szDataJSON, pError));
    free(szDataJSON);
    szDataJSON = NULL;

    // TODO: create sync info in this directory

exit:
    if (szDataJSON) free(szDataJSON);
    if (szFilename) free(szFilename);

    return cc;
}

// finds the next available account number (the number is just used for the directory name)
static
tABC_CC ABC_AccountNextAccountNum(int *pAccountNum,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char *szAccountRoot = NULL;
    char *szAccountDir = NULL;

    ABC_CHECK_NULL(pAccountNum);
    
    // make sure the accounts directory is in place
    ABC_CHECK_RET(ABC_AccountCreateRootDir(pError));
    
    // get the account root directory string
    szAccountRoot = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(szAccountRoot, pError));
    
    // run through all the account names
    szAccountDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    int AccountNum;
    for (AccountNum = 0; AccountNum < ACCOUNT_MAX; AccountNum++)
    {

        ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, AccountNum, pError));
        if (true != ABC_FileIOFileExist(szAccountDir))
        {
            break;
        }
    }
    
    // if we went to the end
    if (AccountNum == ACCOUNT_MAX)
    {
        ABC_RET_ERROR(ABC_CC_NoAvailAccountSpace, "No account space available"); 
    }
    
    *pAccountNum = AccountNum;

exit:
    if (szAccountRoot)  free(szAccountRoot);
    if (szAccountDir)   free(szAccountDir);
    
    return cc;
}

// creates the account directory if needed
static
tABC_CC ABC_AccountCreateRootDir(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char *szAccountRoot = NULL;

    // create the account directory string
    szAccountRoot = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(szAccountRoot, pError));
    
    // if it doesn't exist
    if (ABC_FileIOFileExist(szAccountRoot) != true)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szAccountRoot, pError));
    }
    
exit:
    if (szAccountRoot) free(szAccountRoot);
    
    return cc;
}

static
tABC_CC ABC_AccountCopyRootDirName(char *szRootDir, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szRootDir);
    
    const char *szFileIORootDir;
    ABC_CHECK_RET(ABC_FileIOGetRootDir(&szFileIORootDir, pError));
    
    // create the account directory string
    sprintf(szRootDir, "%s/%s", szFileIORootDir, ACCOUNT_DIR);
    
exit:

    return cc;
}

// gets the account directory for a given username
// the string is allocated so it is up to the caller to free it
tABC_CC ABC_AccountGetDirName(const char *szUserName, char **pszDirName, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char *szDirName = NULL;
    
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(pszDirName);
    
    int accountNum = -1;
    
    // check locally for the account
    ABC_CHECK_RET(ABC_AccountNumForUser(szUserName, &accountNum, pError));
    if (accountNum < 0)
    {
        ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "No account by that name");
    }
    
    // get the account root directory string
    szDirName = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szDirName, accountNum, pError));
    *pszDirName = szDirName;
    szDirName = NULL; // so we don't free it
    
    
exit:
    if (szDirName)  free(szDirName);
    
    return cc;
}

// gets the account sync directory for a given username
// the string is allocated so it is up to the caller to free it
tABC_CC ABC_AccountGetSyncDirName(const char *szUserName,
                                  char **pszDirName,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char *szDirName = NULL;
    
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(pszDirName);
    
    ABC_CHECK_RET(ABC_AccountGetDirName(szUserName, &szDirName, pError));
    
    *pszDirName = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszDirName, "%s/%s", szDirName, ACCOUNT_SYNC_DIR);
    
exit:
    if (szDirName)  free(szDirName);
    
    return cc;
}

// copies the account directory name into the string given
static
tABC_CC ABC_AccountCopyAccountDirName(char *szAccountDir, int AccountNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char *szAccountRoot = NULL;

    ABC_CHECK_NULL(szAccountDir);
    
    // get the account root directory string
    szAccountRoot = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(szAccountRoot, pError));
    
    // create the account directory string
    sprintf(szAccountDir, "%s/%s%d", szAccountRoot, ACCOUNT_FOLDER_PREFIX, AccountNum);
    
exit:
    if (szAccountRoot)  free(szAccountRoot);

    return cc;
}

// creates the json for a list of items in a string seperated by newlines
// for example:
//   "A\nB\n"
// becomes
//  { "name" : [ "A", "B" ] }
static
tABC_CC ABC_AccountCreateListJSON(const char *szName, const char *szItems, char **pszJSON,  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *jsonItems = NULL;
    json_t *jsonItemArray = NULL;
    char *szNewItems = NULL;

    ABC_CHECK_NULL(szName);
    ABC_CHECK_NULL(szItems);
    ABC_CHECK_NULL(pszJSON);

    // create the json object that will be our questions
    jsonItems = json_object();
    jsonItemArray = json_array();

    if (strlen(szItems))
    {
        // change all the newlines into nulls to create a string of them
        szNewItems = strdup(szItems);
        int nItems = 1;
        for (int i = 0; i < strlen(szItems); i++)
        {
            if (szNewItems[i] == '\n')
            {
                nItems++;
                szNewItems[i] = '\0';
            }
        }

        // for each item
        char *pCurItem = szNewItems;
        for (int i = 0; i < nItems; i++)
        {
            json_array_append_new(jsonItemArray, json_string(pCurItem));
            pCurItem += strlen(pCurItem) + 1;
        }
    }

    // set our final json for the questions
    json_object_set(jsonItems, szName, jsonItemArray);

    *pszJSON = json_dumps(jsonItems, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

exit:
    if (jsonItems)      json_decref(jsonItems);
    if (jsonItemArray)  json_decref(jsonItemArray);
    if (szNewItems)     free(szNewItems);

    return cc;
}


// returns the account number associated with the given user name
// -1 is returned if the account does not exist
static 
tABC_CC ABC_AccountNumForUser(const char *szUserName, int *pAccountNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szCurUserName = NULL;
    char *szAccountRoot = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(pAccountNum);

    // assume we didn't find it
    *pAccountNum = -1;
    
    // make sure the accounts directory is in place
    ABC_CHECK_RET(ABC_AccountCreateRootDir(pError));
    
    // get the account root directory string
    szAccountRoot = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(szAccountRoot, pError));

    // get all the files in this root
    tABC_FileIOList *pFileList;
    ABC_FileIOCreateFileList(&pFileList, szAccountRoot, NULL);
    for (int i = 0; i < pFileList->nCount; i++)
    {
        // if this file is a directory
        if (pFileList->apFiles[i]->type == ABC_FILEIOFileType_Directory)
        {
            // if this directory starts with the right prefix
            if ((strlen(pFileList->apFiles[i]->szName) > strlen(ACCOUNT_FOLDER_PREFIX)) &&
                (strncmp(ACCOUNT_FOLDER_PREFIX, pFileList->apFiles[i]->szName, strlen(ACCOUNT_FOLDER_PREFIX)) == 0))
            {
                char *szAccountNum = (char *)(pFileList->apFiles[i]->szName + strlen(ACCOUNT_FOLDER_PREFIX));
                unsigned int AccountNum = (unsigned int) strtol(szAccountNum, NULL, 10); // 10 is for base-10

                // get the username for this account
                ABC_CHECK_RET(ABC_AccountUserForNum(AccountNum, &szCurUserName, pError));

                // if this matches what we are looking for
                if (strcmp(szUserName, szCurUserName) == 0)
                {
                    *pAccountNum = AccountNum;
                    break;
                }
                free(szCurUserName);
                szCurUserName = NULL;
            }
        }
    }
    ABC_FileIOFreeFileList(pFileList, NULL);
    
exit:
    if (szCurUserName)  free(szCurUserName);
    if (szAccountRoot)  free(szAccountRoot);

    return cc;
}

// gets the user name for the specified account number
// name must be free'd by caller
static
tABC_CC ABC_AccountUserForNum(unsigned int AccountNum, char **pszUserName, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *root = NULL;
    char *szAccountNameJSON = NULL;
    char *szAccountRoot = NULL;
    char *szAccountNamePath = NULL;

    ABC_CHECK_NULL(pszUserName);

    // make sure the accounts directory is in place
    ABC_CHECK_RET(ABC_AccountCreateRootDir(pError));
    
    // get the account root directory string
    szAccountRoot = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(szAccountRoot, pError));

    // create the path to the account name file
    szAccountNamePath = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szAccountNamePath, "%s/%s%d/%s", szAccountRoot, ACCOUNT_FOLDER_PREFIX, AccountNum, ACCOUNT_NAME_FILENAME);

    // read in the json
    ABC_CHECK_RET(ABC_FileIOReadFileStr(szAccountNamePath, &szAccountNameJSON, pError));

    // parse out the user name
    json_error_t error;
    root = json_loads(szAccountNameJSON, 0, &error);
    ABC_CHECK_ASSERT(root != NULL, ABC_CC_JSONError, "Error parsing JSON account name");
    ABC_CHECK_ASSERT(json_is_object(root), ABC_CC_JSONError, "Error parsing JSON account name");

    json_t *jsonVal = json_object_get(root, JSON_ACCT_USERNAME_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON account name");
    const char *szUserName = json_string_value(jsonVal);

    *pszUserName = strdup(szUserName);

exit:
    if (root)               json_decref(root);
    if (szAccountNameJSON)  free(szAccountNameJSON);
    if (szAccountRoot)      free(szAccountRoot);
    if (szAccountNamePath)  free(szAccountNamePath);

    return cc;
}

// clears all the keys from the cache
tABC_CC ABC_AccountClearKeyCache(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    if ((gAccountKeysCacheCount > 0) && (NULL != gaAccountKeysCacheArray))
    {
        for (int i = 0; i < gAccountKeysCacheCount; i++)
        {
            tAccountKeys *pAccountKeys = gaAccountKeysCacheArray[i];
            ABC_AccountFreeAccountKeys(pAccountKeys);
        }

        free(gaAccountKeysCacheArray);
        gAccountKeysCacheCount = 0;
    }

exit:

    return cc;
}

// frees all the elements in the given AccountKeys struct
static void ABC_AccountFreeAccountKeys(tAccountKeys *pAccountKeys)
{
    if (pAccountKeys)
    {
        if (pAccountKeys->szUserName)
        {
            memset (pAccountKeys->szUserName, 0, strlen(pAccountKeys->szUserName));
            free(pAccountKeys->szUserName);
            pAccountKeys->szUserName = NULL;
        }

        if (pAccountKeys->szPassword)
        {
            memset (pAccountKeys->szPassword, 0, strlen(pAccountKeys->szPassword));
            free(pAccountKeys->szPassword);
            pAccountKeys->szPassword = NULL;
        }

        if (pAccountKeys->szPIN)
        {
            memset (pAccountKeys->szPIN, 0, strlen(pAccountKeys->szPIN));
            free(pAccountKeys->szPIN);
            pAccountKeys->szPIN = NULL;
        }

        ABC_CryptoFreeSNRP(&(pAccountKeys->pSNRP1));
        ABC_CryptoFreeSNRP(&(pAccountKeys->pSNRP2));
        ABC_CryptoFreeSNRP(&(pAccountKeys->pSNRP3));
        ABC_CryptoFreeSNRP(&(pAccountKeys->pSNRP4));
        ABC_BUF_ZERO(pAccountKeys->L);
        ABC_BUF_FREE(pAccountKeys->L);
        ABC_BUF_ZERO(pAccountKeys->L1);
        ABC_BUF_FREE(pAccountKeys->L1);
        ABC_BUF_ZERO(pAccountKeys->P);
        ABC_BUF_FREE(pAccountKeys->P);
        ABC_BUF_ZERO(pAccountKeys->P1);
        ABC_BUF_FREE(pAccountKeys->P1);
        ABC_BUF_ZERO(pAccountKeys->LRA);
        ABC_BUF_FREE(pAccountKeys->LRA);
        ABC_BUF_ZERO(pAccountKeys->LRA1);
        ABC_BUF_FREE(pAccountKeys->LRA1);
        ABC_BUF_ZERO(pAccountKeys->L2);
        ABC_BUF_FREE(pAccountKeys->L2);
        ABC_BUF_ZERO(pAccountKeys->RQ);
        ABC_BUF_FREE(pAccountKeys->RQ);
        ABC_BUF_ZERO(pAccountKeys->LP);
        ABC_BUF_FREE(pAccountKeys->LP);
        ABC_BUF_ZERO(pAccountKeys->LP2);
        ABC_BUF_FREE(pAccountKeys->LP2);
        ABC_BUF_ZERO(pAccountKeys->LRA2);
        ABC_BUF_FREE(pAccountKeys->LRA2);
    }
}

// adds the given AccountKey to the array of cached account keys
static tABC_CC ABC_AccountAddToKeyCache(tAccountKeys *pAccountKeys, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(pAccountKeys);

    // see if it exists first
    tAccountKeys *pExistingAccountKeys = NULL;
    ABC_CHECK_RET(ABC_AccountKeyFromCacheByName(pAccountKeys->szUserName, &pExistingAccountKeys, pError));

    // if it doesn't currently exist in the array
    if (pExistingAccountKeys == NULL)
    {
        // if we don't have an array yet
        if ((gAccountKeysCacheCount == 0) || (NULL == gaAccountKeysCacheArray))
        {
            // create a new one
            gAccountKeysCacheCount = 0;
            gaAccountKeysCacheArray = malloc(sizeof(tAccountKeys *));
        }
        else 
        {
            // extend the current one
            gaAccountKeysCacheArray = realloc(gaAccountKeysCacheArray, sizeof(tAccountKeys *) * (gAccountKeysCacheCount + 1));
            
        }
        gaAccountKeysCacheArray[gAccountKeysCacheCount] = pAccountKeys;
        gAccountKeysCacheCount++;
    }
    else 
    {
        cc = ABC_CC_AccountAlreadyExists;
    }

exit:

    return cc;
}

// searches for a key in the cached by account name
// if it is not found, the account keys will be set to NULL
static tABC_CC ABC_AccountKeyFromCacheByName(const char *szUserName, tAccountKeys **ppAccountKeys, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(ppAccountKeys);

    // assume we didn't find it
    *ppAccountKeys = NULL;

    if ((gAccountKeysCacheCount > 0) && (NULL != gaAccountKeysCacheArray))
    {
        for (int i = 0; i < gAccountKeysCacheCount; i++)
        {
            tAccountKeys *pAccountKeys = gaAccountKeysCacheArray[i];
            if (0 == strcmp(szUserName, pAccountKeys->szUserName))
            {
                // found it
                *ppAccountKeys = pAccountKeys;
                break;
            }
        }
    }   

exit:

    return cc;
}

// Adds the given user to the key cache
// If the user is not currently in the cache, szUserName, szPassword, szPIN, L, P, LP2, SNRP2, SNRP3, SNRP4  keys are retrieved and the entry is added
// (the initial keys are added so the password can be verified while trying to decrypt EPIN)
static tABC_CC ABC_AccountCacheKeys(const char *szUserName, const char *szPassword, tAccountKeys **ppKeys, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tAccountKeys *pKeys         = NULL;
    char         *szFilename    = NULL;
    char         *szAccountDir  = NULL;
    json_t       *pJSON_ERQ     = NULL;
    json_t       *pJSON_SNRP2   = NULL;
    json_t       *pJSON_SNRP3   = NULL;
    json_t       *pJSON_SNRP4   = NULL;
    char         *szJSON_EPIN   = NULL;
    tABC_U08Buf  PIN            = ABC_BUF_NULL;
    json_t       *pJSON_Root    = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);

    // see if it is already in the cache
    ABC_CHECK_RET(ABC_AccountKeyFromCacheByName(szUserName, &pKeys, pError));

    // if it is already cached
    if (NULL != pKeys)
    {
        // hang on to these keys
        *ppKeys = pKeys;

        // if we don't do this, we'll free it below but it isn't ours, it is from the cache
        pKeys = NULL; 

        // if the password doesn't match
        char *szOriginalPassword = (*ppKeys)->szPassword;
        if (0 != strcmp(szOriginalPassword, szPassword))
        {
            ABC_RET_ERROR(ABC_CC_BadPassword, "Incorrect password for account");
        }
    }
    else
    {
        // we need to add it

        // check if the account exists
        int AccountNum = -1;
        ABC_CHECK_RET(ABC_AccountNumForUser(szUserName, &AccountNum, pError));
        if (AccountNum >= 0)
        {
            pKeys = calloc(1, sizeof(tAccountKeys));
            pKeys->accountNum = AccountNum;
            pKeys->szUserName = strdup(szUserName);
            pKeys->szPassword = strdup(szPassword);

            ABC_CHECK_RET(ABC_AccountGetCarePackageObjects(AccountNum, NULL, &pJSON_SNRP2, &pJSON_SNRP3, &pJSON_SNRP4, pError));

            // SNRP's
            ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP2, &(pKeys->pSNRP2), pError));
            ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP2, &(pKeys->pSNRP3), pError));
            ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP2, &(pKeys->pSNRP4), pError));

            // L = username
            ABC_BUF_DUP_PTR(pKeys->L, pKeys->szUserName, strlen(pKeys->szUserName));

            // P = password
            ABC_BUF_DUP_PTR(pKeys->P, pKeys->szPassword, strlen(pKeys->szPassword));

            // LP = L + P
            ABC_BUF_DUP(pKeys->LP, pKeys->L);
            ABC_BUF_APPEND(pKeys->LP, pKeys->P);

            // LP2 = Scrypt(L + P, SNRP2)
            ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->LP, pKeys->pSNRP2, &(pKeys->LP2), pError));

            // load EPIN
            szAccountDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
            ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, pKeys->accountNum, pError));
            szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
            sprintf(szFilename, "%s/%s", szAccountDir, ACCOUNT_EPIN_FILENAME);
            ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, &szJSON_EPIN, pError));

            // try to decrypt
            tABC_CC CC_Decrypt = ABC_CryptoDecryptJSONString(szJSON_EPIN, pKeys->LP2, &(PIN), pError);
            if (CC_Decrypt != ABC_CC_Ok)
            {
                // a little bit of an assumption here is that we couldn't decrypt because the password was bad
                ABC_RET_ERROR(ABC_CC_BadPassword, "Could not decrypt PIN - possibly bad password");
            }

            // decode the json to get the pin
            char *szJSON_PIN = (char *) ABC_BUF_PTR(PIN);
            ABC_CHECK_RET(ABC_UtilGetStringValueFromJSONString(szJSON_PIN, JSON_ACCT_PIN_FIELD, &(pKeys->szPIN), pError));

            // add new starting account keys to the key cache
            ABC_CHECK_RET(ABC_AccountAddToKeyCache(pKeys, pError));
            *ppKeys = pKeys;
            pKeys = NULL; // so we don't free it at the end

        }
        else 
        {
            ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "No account by that name");
        }
    }

exit:
    if (pKeys)              
    {
        ABC_AccountFreeAccountKeys(pKeys);
        free(pKeys);
    }
    if (szFilename)     free(szFilename);
    if (szAccountDir)   free(szAccountDir);
    if (pJSON_ERQ)      json_decref(pJSON_ERQ);
    if (pJSON_SNRP2)    json_decref(pJSON_SNRP2);
    if (pJSON_SNRP3)    json_decref(pJSON_SNRP3);
    if (pJSON_SNRP4)    json_decref(pJSON_SNRP4);
    if (pJSON_Root)     json_decref(pJSON_Root);
    if (szJSON_EPIN)    free(szJSON_EPIN);
    ABC_BUF_FREE(PIN);

    return cc;
}

// retrieves the specified key from the key cache
// if the account associated with the username and password is not currently in the cache, it is added
tABC_CC ABC_AccountGetKey(const char *szUserName, const char *szPassword, tABC_AccountKey keyType, tABC_U08Buf *pKey, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tAccountKeys *pKeys = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(pKey);

    // make sure the account is in the cache
    ABC_CHECK_RET(ABC_AccountCacheKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_ASSERT(NULL != pKeys, ABC_CC_Error, "Expected to find account keys in cache.");

    switch(keyType)
    {
        case ABC_AccountKey_L2:
            // L2 = Scrypt(L, SNRP4)
            if (NULL == ABC_BUF_PTR(pKeys->L2))
            {
                ABC_CHECK_ASSERT(NULL != ABC_BUF_PTR(pKeys->L), ABC_CC_Error, "Expected to find L in key cache");
                ABC_CHECK_ASSERT(NULL != pKeys->pSNRP4, ABC_CC_Error, "Expected to find SNRP4 in key cache");
                ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->L, pKeys->pSNRP4, &(pKeys->L2), pError));
            }
            ABC_BUF_SET(*pKey, pKeys->L2);
            break;

        case ABC_AccountKey_LP2:
            // this should already be in the cache
            ABC_CHECK_ASSERT(NULL != ABC_BUF_PTR(pKeys->LP2), ABC_CC_Error, "Expected to find LP2 in key cache");
            ABC_BUF_SET(*pKey, pKeys->LP2);
            break;
            
        case ABC_AccountKey_PIN:
            // this should already be in the cache
            ABC_CHECK_ASSERT(NULL != pKeys->szPIN, ABC_CC_Error, "Expected to find PIN in key cache");
            ABC_BUF_SET_PTR(*pKey, (unsigned char *)pKeys->szPIN, sizeof(pKeys->szPIN) + 1);
            break;

        default:
            ABC_RET_ERROR(ABC_CC_Error, "Unknown key type");
            break;
            
    };

exit:

    return cc;
}

tABC_CC ABC_AccountSetPIN(const char *szUserName,
                          const char *szPassword,
                          const char *szPIN,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    tAccountKeys *pKeys = NULL;
    tABC_U08Buf LP2 = ABC_BUF_NULL;
    char *szJSON = NULL;
    char *szEPIN_JSON = NULL;
    char *szAccountDir = NULL;
    char *szFilename = NULL;
    
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szPIN);
    
    // get LP2 (this will also validate username and password as well as cache the account)
    ABC_CHECK_RET(ABC_AccountGetKey(szUserName, szPassword, ABC_AccountKey_LP2, &LP2, pError));
    
    // get the key cache
    ABC_CHECK_RET(ABC_AccountCacheKeys(szUserName, szPassword, &pKeys, pError));
    
    // set the new PIN in the cache
    free(pKeys->szPIN);
    pKeys->szPIN = strdup(szPIN);
    
    // create the PIN JSON
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(pKeys->szPIN, JSON_ACCT_PIN_FIELD, &szJSON, pError));
    tABC_U08Buf PIN = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(PIN, (unsigned char *)szJSON, strlen(szJSON) + 1);
    
    // EPIN = AES256(PIN, LP2)
    ABC_CHECK_RET(ABC_CryptoEncryptJSONString(PIN, pKeys->LP2, ABC_CryptoType_AES256, &szEPIN_JSON, pError));
    
    // write the EPIN
    ABC_CHECK_RET(ABC_AccountGetDirName(szUserName, &szAccountDir, pError));
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", szAccountDir, ACCOUNT_EPIN_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szEPIN_JSON, pError));
    
exit:
    if (szJSON) free(szJSON);
    if (szEPIN_JSON) free(szEPIN_JSON);
    if (szAccountDir) free(szAccountDir);
    if (szFilename) free(szFilename);
    
    return cc;
}

// This function gets the categories for an account.
// An array of allocated strings is allocated so the user is responsible for
// free'ing all the elements as well as the array itself.
tABC_CC ABC_AccountGetCategories(const char *szUserName,
                                 char ***paszCategories,
                                 unsigned int *pCount,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char *szAccountDir = NULL;
    char *szFilename = NULL;
    char *szJSON = NULL;
    
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(paszCategories);
    *paszCategories = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;
    
    // load the categories
    ABC_CHECK_RET(ABC_AccountGetDirName(szUserName, &szAccountDir, pError));
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s/%s", szAccountDir, ACCOUNT_SYNC_DIR, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, &szJSON, pError));
    
    // load the strings of values
    ABC_CHECK_RET(ABC_UtilGetArrayValuesFromJSONString(szJSON, JSON_ACCT_CATEGORIES_FIELD, paszCategories, pCount, pError));
    
exit:
    if (szJSON) free(szJSON);
    if (szAccountDir) free(szAccountDir);
    if (szFilename) free(szFilename);
    
    return cc;
}

// This function adds a category to an account.
// No attempt is made to avoid a duplicate entry.
tABC_CC ABC_AccountAddCategory(const char *szUserName,
                               char *szCategory,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char **aszCategories = NULL;
    unsigned int categoryCount = 0;
    
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szCategory);
    
    // load the current categories
    ABC_CHECK_RET(ABC_AccountGetCategories(szUserName, &aszCategories, &categoryCount, pError));
    
    // if there are categories
    if ((aszCategories != NULL) && (categoryCount > 0))
    {
        aszCategories = realloc(aszCategories, sizeof(char *) * (categoryCount + 1));
    }
    else
    {
        aszCategories = malloc(sizeof(char *));
        categoryCount = 0;
    }
    aszCategories[categoryCount] = strdup(szCategory);
    categoryCount++;
    
    // save out the categories
    ABC_CHECK_RET(ABC_AccountSaveCategories(szUserName, aszCategories, categoryCount, pError));
    
exit:
    ABC_UtilFreeStringArray(aszCategories, categoryCount);
    
    return cc;
}

// This function removes a category from an account.
// If there is more than one category with this name, all categories by this name are removed.
// If the category does not exist, no error is returned.
tABC_CC ABC_AccountRemoveCategory(const char *szUserName,
                                  char *szCategory,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char **aszCategories = NULL;
    unsigned int categoryCount = 0;
    char **aszNewCategories = NULL;
    unsigned int newCategoryCount = 0;
    
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szCategory);
    
    // load the current categories
    ABC_CHECK_RET(ABC_AccountGetCategories(szUserName, &aszCategories, &categoryCount, pError));
    
    // got through all the categories and only add ones that are not this one
    for (int i = 0; i < categoryCount; i++)
    {
        // if this is not the string we are looking to remove then add it to our new arrary
        if (0 != strcmp(aszCategories[i], szCategory))
        {
            // if there are categories
            if ((aszNewCategories != NULL) && (newCategoryCount > 0))
            {
                aszNewCategories = realloc(aszNewCategories, sizeof(char *) * (newCategoryCount + 1));
            }
            else
            {
                aszNewCategories = malloc(sizeof(char *));
                newCategoryCount = 0;
            }
            aszNewCategories[newCategoryCount] = strdup(aszCategories[i]);
            newCategoryCount++;
        }
    }
    
    // save out the new categories
    ABC_CHECK_RET(ABC_AccountSaveCategories(szUserName, aszNewCategories, newCategoryCount, pError));

exit:
    ABC_UtilFreeStringArray(aszCategories, categoryCount);
    ABC_UtilFreeStringArray(aszNewCategories, newCategoryCount);
    
    return cc;
}

// saves the categories for the given account
static
tABC_CC ABC_AccountSaveCategories(const char *szUserName,
                                  char **aszCategories,
                                  unsigned int count,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char *szDataJSON = NULL;
    char *szFilename = NULL;
    char *szAccountDir = NULL;
    
    ABC_CHECK_NULL(szUserName);
    
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    
    // create the categories JSON
    ABC_CHECK_RET(ABC_UtilCreateArrayJSONString(aszCategories, count, JSON_ACCT_CATEGORIES_FIELD, &szDataJSON, pError));
    
    // write them out
    ABC_CHECK_RET(ABC_AccountGetDirName(szUserName, &szAccountDir, pError));
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s/%s", szAccountDir, ACCOUNT_SYNC_DIR, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szDataJSON, pError));
    
exit:
    if (szDataJSON) free(szDataJSON);
    if (szFilename) free(szFilename);
    if (szAccountDir) free(szAccountDir);
    
    return cc;
}
