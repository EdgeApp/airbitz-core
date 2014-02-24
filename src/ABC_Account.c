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
#include "ABC_URL.h"
#include "ABC_Debug.h"
#include "ABC_ServerDefs.h"

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
#define ACCOUNT_QUESTIONS_FILENAME      "Questions.json"

#define JSON_ACCT_USERNAME_FIELD        "userName"
#define JSON_ACCT_PIN_FIELD             "PIN"
#define JSON_ACCT_QUESTIONS_FIELD       "questions"
#define JSON_ACCT_WALLETS_FIELD         "wallets"
#define JSON_ACCT_CATEGORIES_FIELD      "categories"
#define JSON_ACCT_ERQ_FIELD             "ERQ"
#define JSON_ACCT_SNRP_FIELD_PREFIX     "SNRP"
#define JSON_ACCT_QUESTIONS_FIELD       "questions"


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
static tABC_CC ABC_AccountServerCreate(tABC_U08Buf L1, tABC_U08Buf P1, tABC_Error *pError);
static tABC_CC ABC_AccountSetRecovery(tABC_AccountSetRecoveryInfo *pInfo, tABC_Error *pError);
static tABC_CC ABC_AccountServerSetRecovery(tABC_U08Buf L1, tABC_U08Buf P1, tABC_U08Buf LRA1, const char *szCarePackage, tABC_Error *pError);
static tABC_CC ABC_AccountCreateCarePackageJSONString(const json_t *pJSON_ERQ, const json_t *pJSON_SNRP2, const json_t *pJSON_SNRP3, const json_t *pJSON_SNRP4, char **pszJSON, tABC_Error *pError);
static tABC_CC ABC_AccountGetCarePackageObjects(int AccountNum, json_t **ppJSON_ERQ, json_t **ppJSON_SNRP2, json_t **ppJSON_SNRP3, json_t **ppJSON_SNRP4, tABC_Error *pError);
static tABC_CC ABC_AccountCreateSync(const char *szAccountsRootDir, tABC_Error *pError);
static tABC_CC ABC_AccountNextAccountNum(int *pAccountNum, tABC_Error *pError);
static tABC_CC ABC_AccountCreateRootDir(tABC_Error *pError);
static tABC_CC ABC_AccountGetRootDir(char **pszRootDir, tABC_Error *pError);
static tABC_CC ABC_AccountCopyRootDirName(char *szRootDir, tABC_Error *pError);
static tABC_CC ABC_AccountCopyAccountDirName(char *szAccountDir, int AccountNum, tABC_Error *pError);
static tABC_CC ABC_AccountCreateListJSON(const char *szName, const char *szItems, char **pszJSON,  tABC_Error *pError);
static tABC_CC ABC_AccountNumForUser(const char *szUserName, int *pAccountNum, tABC_Error *pError);
static tABC_CC ABC_AccountUserForNum(unsigned int AccountNum, char **pszUserName, tABC_Error *pError);
static tABC_CC ABC_AccountCacheKeys(const char *szUserName, const char *szPassword, tAccountKeys **ppKeys, tABC_Error *pError);
static void    ABC_AccountFreeAccountKeys(tAccountKeys *pAccountKeys);
static tABC_CC ABC_AccountAddToKeyCache(tAccountKeys *pAccountKeys, tABC_Error *pError);
static tABC_CC ABC_AccountKeyFromCacheByName(const char *szUserName, tAccountKeys **ppAccountKeys, tABC_Error *pError);
static tABC_CC ABC_AccountSaveCategories(const char *szUserName, char **aszCategories, unsigned int Count, tABC_Error *pError);
static tABC_CC ABC_AccountServerGetQuestions(tABC_U08Buf L1, json_t **ppJSON_Q, tABC_Error *pError);
static tABC_CC ABC_AccountUpdateQuestionChoices(const char *szUserName, tABC_Error *pError);
static tABC_CC ABC_AccountGetQuestionChoices(tABC_AccountQuestionsInfo *pInfo, tABC_QuestionChoices **ppQuestionChoices, tABC_Error *pError);

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
        if (pAccountCreateInfo->szUserName)
        {
            memset(pAccountCreateInfo->szUserName, 0, strlen(pAccountCreateInfo->szUserName));

            free((void *)pAccountCreateInfo->szUserName);
        }
        if (pAccountCreateInfo->szPassword)
        {
            memset(pAccountCreateInfo->szPassword, 0, strlen(pAccountCreateInfo->szPassword));

            free((void *)pAccountCreateInfo->szPassword);
        }
        if (pAccountCreateInfo->szPIN)
        {
            memset(pAccountCreateInfo->szPIN, 0, strlen(pAccountCreateInfo->szPIN));

            free((void *)pAccountCreateInfo->szPIN);
        }

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
        if (pAccountSignInInfo->szUserName)
        {
            memset(pAccountSignInInfo->szUserName, 0, strlen(pAccountSignInInfo->szUserName));

            free((void *)pAccountSignInInfo->szUserName);
        }
        if (pAccountSignInInfo->szPassword)
        {
            memset(pAccountSignInInfo->szPassword, 0, strlen(pAccountSignInInfo->szPassword));

            free((void *)pAccountSignInInfo->szPassword);
        }

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
        if (pAccountSetRecoveryInfo->szUserName)
        {
            memset(pAccountSetRecoveryInfo->szUserName, 0, strlen(pAccountSetRecoveryInfo->szUserName));

            free((void *)pAccountSetRecoveryInfo->szUserName);
        }
        if (pAccountSetRecoveryInfo->szPassword)
        {
            memset(pAccountSetRecoveryInfo->szPassword, 0, strlen(pAccountSetRecoveryInfo->szPassword));

            free((void *)pAccountSetRecoveryInfo->szPassword);
        }
        if (pAccountSetRecoveryInfo->szRecoveryQuestions)
        {
            memset(pAccountSetRecoveryInfo->szRecoveryQuestions, 0, strlen(pAccountSetRecoveryInfo->szRecoveryQuestions));

            free((void *)pAccountSetRecoveryInfo->szRecoveryQuestions);
        }
        if (pAccountSetRecoveryInfo->szRecoveryAnswers)
        {
            memset(pAccountSetRecoveryInfo->szRecoveryAnswers, 0, strlen(pAccountSetRecoveryInfo->szRecoveryAnswers));

            free((void *)pAccountSetRecoveryInfo->szRecoveryAnswers);
        }

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

/**
 * Checks if the username and password are valid.
 *
 * If the login info is valid, the keys for this account
 * are also cached.
 * If the creditials are not valid, an error will be returned
 *
 * @param szUserName UserName for validation
 * @param szPassword Password for validation
 */
tABC_CC ABC_AccountCheckCredentials(const char *szUserName,
                                    const char *szPassword,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);

    // check that this is a valid user
    ABC_CHECK_RET(ABC_AccountCheckValidUser(szUserName, pError));

    // cache up the keys
    ABC_CHECK_RET(ABC_AccountCacheKeys(szUserName, szPassword, NULL, pError));

exit:
    
    return cc;
}

/**
 * Checks if the username is valid.
 *
 * If the username is not valid, an error will be returned
 *
 * @param szUserName UserName for validation
 */
tABC_CC ABC_AccountCheckValidUser(const char *szUserName,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);

    int AccountNum = 0;

    // check locally for the account
    ABC_CHECK_RET(ABC_AccountNumForUser(szUserName, &AccountNum, pError));
    if (AccountNum < 0)
    {
        ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "No account by that name");
    }

exit:

    return cc;
}

// sign in to an account
// this cache's the keys for an account
static
tABC_CC ABC_AccountSignIn(tABC_AccountSignInInfo *pInfo,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(pInfo);

    // check the credentials
    ABC_CHECK_RET(ABC_AccountCheckCredentials(pInfo->szUserName, pInfo->szPassword, pError));

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
    //ABC_UtilHexDumpBuf("L1", pKeys->L1);

    // P = password
    ABC_BUF_DUP_PTR(pKeys->P, pKeys->szPassword, strlen(pKeys->szPassword));
    //ABC_UtilHexDumpBuf("P", pKeys->P);

    // P1 = Scrypt(P, SNRP1)
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->P, pKeys->pSNRP1, &(pKeys->P1), pError));
    //ABC_UtilHexDumpBuf("P1", pKeys->P1);

    // CarePackage = ERQ, SNRP2, SNRP3, SNRP4
    ABC_CHECK_RET(ABC_AccountCreateCarePackageJSONString(NULL, pJSON_SNRP2, pJSON_SNRP3, pJSON_SNRP4, &szCarePackage_JSON, pError));

    // TODO: create RepoAcctKey and ERepoAcctKey

    // check with the server that the account name is available while also sending the data it will need
    // TODO: need to add ERepoAcctKey to the server
    ABC_CHECK_RET(ABC_AccountServerCreate(pKeys->L1, pKeys->P1, pError));

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

    // take this opportunity to download the questions they can choose from for recovery
    ABC_CHECK_RET(ABC_AccountUpdateQuestionChoices(pInfo->szUserName, pError));

exit:
    if (pKeys)
    {
        ABC_AccountFreeAccountKeys(pKeys);
        free(pKeys);
    }
    if (pJSON_SNRP2)        json_decref(pJSON_SNRP2);
    if (pJSON_SNRP3)        json_decref(pJSON_SNRP3);
    if (pJSON_SNRP4)        json_decref(pJSON_SNRP4);
    if (szCarePackage_JSON) free(szCarePackage_JSON);
    if (szEPIN_JSON)        free(szEPIN_JSON);
    if (szJSON)             free(szJSON);
    if (szAccountDir)       free(szAccountDir);
    if (szFilename)         free(szFilename);

    return cc;
}

/**
 * Creates an account on the server.
 *
 * This function sends information to the server to create an account.
 * If the account was created, ABC_CC_Ok is returned.
 * If the account already exists, ABC_CC_AccountAlreadyExists is returned.
 *
 * @param L1   Login hash for the account
 * @param P1   Password hash for the account
 */
static
tABC_CC ABC_AccountServerCreate(tABC_U08Buf L1, tABC_U08Buf P1, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szURL     = NULL;
    char *szResults = NULL;
    char *szPost    = NULL;
    char *szL1_Base64 = NULL;
    char *szP1_Base64 = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL_BUF(P1);


    // create the URL
    szURL = malloc(ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_ACCOUNT_CREATE_PATH);

    // create base64 versions of L1 and P1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(P1, &szP1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{ssss}", ABC_SERVER_JSON_L1_FIELD, szL1_Base64, ABC_SERVER_JSON_P1_FIELD, szP1_Base64);
    szPost = json_dumps(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %s", szURL, szPost);

    // send the command
    //ABC_CHECK_RET(ABC_URLPostString("http://httpbin.org/post", szPost, &szResults, pError));
    //ABC_DebugLog("Results: %s", szResults);
    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %s", szResults);

    // decode the result
    json_t *pJSON_Value = NULL;
    json_error_t error;
    pJSON_Root = json_loads(szResults, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing server JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // get the status code
    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_STATUS_CODE_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_number(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON status code");
    int statusCode = (int) json_integer_value(pJSON_Value);

    // if there was a failure
    if (ABC_Server_Code_Success != statusCode)
    {
        if (ABC_Server_Code_AccountExists == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_AccountAlreadyExists, "Account already exists on server");
        }
        else
        {
            // get the message
            pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_MESSAGE_FIELD);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_DebugLog("Server message: %s", json_string_value(pJSON_Value));
            ABC_RET_ERROR(ABC_CC_ServerError, json_string_value(pJSON_Value));
        }
    }

exit:
    if (szURL)          free(szURL);
    if (szResults)      free(szResults);
    if (szPost)         free(szPost);
    if (szL1_Base64)    free(szL1_Base64);
    if (szP1_Base64)    free(szP1_Base64);
    if (pJSON_Root)     json_decref(pJSON_Root);
    
    return cc;
}

/**
 * Set the recovery questions for an account
 *
 * This function sets the password recovery informatin for the account.
 * This includes sending a new care package to the server
 *
 * @param pInfo     Pointer to recovery information data
 * @param pError    A pointer to the location to store the error if there is one
 */
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
    //ABC_DEBUG(ABC_UtilHexDumpBuf("LRA", pKeys->LRA));

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

    // Client sends L1, P1, LRA1, CarePackage, to the server
    ABC_CHECK_RET(ABC_AccountServerSetRecovery(pKeys->L1, pKeys->P1, pKeys->LRA1, szCarePackage_JSON, pError));

exit:
    if (pJSON_ERQ)          json_decref(pJSON_ERQ);
    if (pJSON_SNRP2)        json_decref(pJSON_SNRP2);
    if (pJSON_SNRP3)        json_decref(pJSON_SNRP3);
    if (pJSON_SNRP4)        json_decref(pJSON_SNRP4);
    if (szCarePackage_JSON) free(szCarePackage_JSON);
    if (szELP2_JSON)        free(szELP2_JSON);
    if (szELRA2_JSON)       free(szELRA2_JSON);
    if (szAccountDir)       free(szAccountDir);
    if (szFilename)         free(szFilename);

    return cc;
}

/**
 * Set recovery questions and answers on the server.
 *
 * This function sends LRA1 and Care Package to the server as part
 * of setting up the recovery data for an account
 *
 * @param L1            Login hash for the account
 * @param P1            Password hash for the account
 * @param LRA1          Scrypt'ed login and recovery answers
 * @param szCarePackage Care Package for account
 */
static
tABC_CC ABC_AccountServerSetRecovery(tABC_U08Buf L1, tABC_U08Buf P1, tABC_U08Buf LRA1, const char *szCarePackage, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szURL     = NULL;
    char *szResults = NULL;
    char *szPost    = NULL;
    char *szL1_Base64 = NULL;
    char *szP1_Base64 = NULL;
    char *szLRA1_Base64 = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL_BUF(P1);
    ABC_CHECK_NULL_BUF(LRA1);
    ABC_CHECK_NULL(szCarePackage);

    // create the URL
    szURL = malloc(ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_UPDATE_CARE_PACKAGE_PATH);

    // create base64 versions of L1, P1 and LRA1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(P1, &szP1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LRA1, &szLRA1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{ssssssss}",
                           ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                           ABC_SERVER_JSON_P1_FIELD, szP1_Base64,
                           ABC_SERVER_JSON_LRA1_FIELD, szLRA1_Base64,
                           ABC_SERVER_JSON_CARE_PACKAGE_FIELD, szCarePackage);
    szPost = json_dumps(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %s", szURL, szPost);

    // send the command
    //ABC_CHECK_RET(ABC_URLPostString("http://httpbin.org/post", szPost, &szResults, pError));
    //ABC_DebugLog("Results: %s", szResults);
    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %s", szResults);

    // decode the result
    json_t *pJSON_Value = NULL;
    json_error_t error;
    pJSON_Root = json_loads(szResults, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing server JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // get the status code
    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_STATUS_CODE_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_number(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON status code");
    int statusCode = (int) json_integer_value(pJSON_Value);

    // if there was a failure
    if (ABC_Server_Code_Success != statusCode)
    {
        if (ABC_Server_Code_NoAccount == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "Account does not exist on server");
        }
        else if (ABC_Server_Code_InvalidPassword == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_BadPassword, "Invalid password on server");
        }
        else
        {
            // get the message
            pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_MESSAGE_FIELD);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_DebugLog("Server message: %s", json_string_value(pJSON_Value));
            ABC_RET_ERROR(ABC_CC_ServerError, json_string_value(pJSON_Value));
        }
    }

exit:
    if (szURL)          free(szURL);
    if (szResults)      free(szResults);
    if (szPost)         free(szPost);
    if (szL1_Base64)    free(szL1_Base64);
    if (szP1_Base64)    free(szP1_Base64);
    if (szLRA1_Base64)  free(szLRA1_Base64);
    if (pJSON_Root)     json_decref(pJSON_Root);
    
    return cc;
}


/**
 * Creates the JSON care package
 *
 * @param pJSON_ERQ    Pointer to ERQ JSON object
 *                     (if this is NULL, ERQ is not added to the care package)
 * @param pJSON_SNRP2  Pointer to SNRP2 JSON object
 * @param pJSON_SNRP3  Pointer to SNRP3 JSON object
 * @param pJSON_SNRP4  Pointer to SNRIP4 JSON object
 * @param pszJSON      Pointer to store allocated JSON for care package.
 *                     (the user is responsible for free'ing this pointer)
 */
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

/**
 * Gets the root account directory
 *
 * @param pszRootDir pointer to store allocated string
 *                   (the user is responsible for free'ing)
 */
static
tABC_CC ABC_AccountGetRootDir(char **pszRootDir, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(pszRootDir);

    // create the account directory string
    *pszRootDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(*pszRootDir, pError));

exit:

    return cc;
}

/**
 * Copies the root account directory into the string given
 *
 * @param szRootDir pointer into which to copy the string
 */
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

// Adds the given user to the key cache if it isn't already cached.
// With or without a password, szUserName, L, SNRP1, SNRP2, SNRP3, SNRP4 keys are retrieved and added if they aren't already in the cache
// If a password is given, szPassword, szPIN, P, LP2 keys are retrieved and the entry is added
//  (the initial keys are added so the password can be verified while trying to decrypt EPIN)
// If a pointer to hold the keys is given, then it is set to those keys
static
tABC_CC ABC_AccountCacheKeys(const char *szUserName, const char *szPassword, tAccountKeys **ppKeys, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tAccountKeys *pKeys         = NULL;
    tAccountKeys *pFinalKeys    = NULL;
    char         *szFilename    = NULL;
    char         *szAccountDir  = NULL;
    json_t       *pJSON_ERQ     = NULL;
    json_t       *pJSON_SNRP2   = NULL;
    json_t       *pJSON_SNRP3   = NULL;
    json_t       *pJSON_SNRP4   = NULL;
    tABC_U08Buf  PIN_JSON       = ABC_BUF_NULL;
    json_t       *pJSON_Root    = NULL;
    tABC_U08Buf  P              = ABC_BUF_NULL;
    tABC_U08Buf  LP             = ABC_BUF_NULL;
    tABC_U08Buf  LP2            = ABC_BUF_NULL;

    ABC_CHECK_NULL(szUserName);

    // see if it is already in the cache
    ABC_CHECK_RET(ABC_AccountKeyFromCacheByName(szUserName, &pFinalKeys, pError));

    // if there wasn't an entry already in the cache - let's add it
    if (NULL == pFinalKeys)
    {
        // we need to add it but start with only those things that require the user name

        // check if the account exists
        int AccountNum = -1;
        ABC_CHECK_RET(ABC_AccountNumForUser(szUserName, &AccountNum, pError));
        if (AccountNum >= 0)
        {
            pKeys = calloc(1, sizeof(tAccountKeys));
            pKeys->accountNum = AccountNum;
            pKeys->szUserName = strdup(szUserName);

            ABC_CHECK_RET(ABC_AccountGetCarePackageObjects(AccountNum, NULL, &pJSON_SNRP2, &pJSON_SNRP3, &pJSON_SNRP4, pError));

            // SNRP's
            ABC_CHECK_RET(ABC_CryptoCreateSNRPForServer(&(pKeys->pSNRP1), pError));
            ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP2, &(pKeys->pSNRP2), pError));
            ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP2, &(pKeys->pSNRP3), pError));
            ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP2, &(pKeys->pSNRP4), pError));

            // L = username
            ABC_BUF_DUP_PTR(pKeys->L, pKeys->szUserName, strlen(pKeys->szUserName));

            // add new starting account keys to the key cache
            ABC_CHECK_RET(ABC_AccountAddToKeyCache(pKeys, pError));
            pFinalKeys = pKeys;
            pKeys = NULL; // so we don't free it at the end

        }
        else
        {
            ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "No account by that name");
        }
    }

    // at this point there is now one in the cache and it is pFinalKeys
    // but it may or may not have password keys

    // if we are given a password
    if (NULL != szPassword)
    {
        // if there is no key in the cache, let's add the keys we can with a password
        if (NULL == pFinalKeys->szPassword)
        {
            // P = password
            ABC_BUF_DUP_PTR(P, szPassword, strlen(szPassword));

            // LP = L + P
            ABC_BUF_DUP(LP, pFinalKeys->L);
            ABC_BUF_APPEND(LP, P);

            // LP2 = Scrypt(L + P, SNRP2)
            ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pFinalKeys->pSNRP2, &LP2, pError));

            // try to decrypt EPIN
            szAccountDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
            ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, pFinalKeys->accountNum, pError));
            szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
            sprintf(szFilename, "%s/%s", szAccountDir, ACCOUNT_EPIN_FILENAME);
            tABC_CC CC_Decrypt = ABC_CryptoDecryptJSONFile(szFilename, LP2, &PIN_JSON, pError);

            // check the results
            if (ABC_CC_DecryptFailure == CC_Decrypt)
            {
                // the assumption here is that this specific error is due to a bad password
                ABC_RET_ERROR(ABC_CC_BadPassword, "Could not decrypt PIN - bad password");
            }
            else if (ABC_CC_Ok != CC_Decrypt)
            {
                // this was an error other than just a bad key so we need to treat this like an error
                cc = CC_Decrypt;
                goto exit;
            }

            // if we got here, then the password was good so we can add what we just calculated to the keys
            pFinalKeys->szPassword = strdup(szPassword);
            ABC_BUF_SET(pFinalKeys->P, P);
            ABC_BUF_CLEAR(P);
            ABC_BUF_SET(pFinalKeys->LP, LP);
            ABC_BUF_CLEAR(LP);
            ABC_BUF_SET(pFinalKeys->LP2, LP2);
            ABC_BUF_CLEAR(LP2);

            // decode the json to get the pin
            char *szJSON_PIN = (char *) ABC_BUF_PTR(PIN_JSON);
            ABC_CHECK_RET(ABC_UtilGetStringValueFromJSONString(szJSON_PIN, JSON_ACCT_PIN_FIELD, &(pFinalKeys->szPIN), pError));
        }
        else
        {
            // make sure it is correct
            if (0 != strcmp(pFinalKeys->szPassword, szPassword))
            {
                ABC_RET_ERROR(ABC_CC_BadPassword, "Password is incorrect");
            }
        }
    }



    // if they wanted the keys
    if (ppKeys)
    {
        *ppKeys = pFinalKeys;
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
    ABC_BUF_FREE(PIN_JSON);
    ABC_BUF_FREE(P);
    ABC_BUF_FREE(LP);
    ABC_BUF_FREE(LP2);

    return cc;
}

// retrieves the specified key from the key cache
// if the account associated with the username and password is not currently in the cache, it is added
tABC_CC ABC_AccountGetKey(const char *szUserName, const char *szPassword, tABC_AccountKey keyType, tABC_U08Buf *pKey, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tAccountKeys *pKeys = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(pKey);

    // make sure the account is in the cache
    ABC_CHECK_RET(ABC_AccountCacheKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_ASSERT(NULL != pKeys, ABC_CC_Error, "Expected to find account keys in cache.");

    switch(keyType)
    {
        case ABC_AccountKey_L1:
            // L1 = Scrypt(L, SNRP1)
            if (NULL == ABC_BUF_PTR(pKeys->L1))
            {
                ABC_CHECK_ASSERT(NULL != ABC_BUF_PTR(pKeys->L), ABC_CC_Error, "Expected to find L in key cache");
                ABC_CHECK_ASSERT(NULL != pKeys->pSNRP1, ABC_CC_Error, "Expected to find SNRP1 in key cache");
                ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->L, pKeys->pSNRP1, &(pKeys->L1), pError));
            }
            ABC_BUF_SET(*pKey, pKeys->L1);
            break;

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

/**
 * Check that the recovery answers for a given account are valid
 * @param pbValid true is stored in here if they are correct
 */
tABC_CC ABC_AccountCheckRecoveryAnswers(const char *szUserName,
                                        const char *szRecoveryAnswers,
                                        bool *pbValid,
                                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tAccountKeys *pKeys = NULL;
    tABC_U08Buf LRA = ABC_BUF_NULL;
    tABC_U08Buf LRA2 = ABC_BUF_NULL;
    tABC_U08Buf LP2 = ABC_BUF_NULL;
    char *szAccountSyncDir = NULL;
    char *szFilename = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_NULL(pbValid);
    *pbValid = false;

    // pull this account into the cache
    ABC_CHECK_RET(ABC_AccountCacheKeys(szUserName, NULL, &pKeys, pError));

    // create our LRA (L + RA) with the answers given
    ABC_BUF_DUP(LRA, pKeys->L);
    ABC_BUF_APPEND_PTR(LRA, szRecoveryAnswers, strlen(szRecoveryAnswers));

    // if the cache has an LRA
    if (ABC_BUF_PTR(pKeys->LRA) != NULL)
    {
        // check if they are equal
        if (ABC_BUF_SIZE(LRA) == ABC_BUF_SIZE(pKeys->LRA))
        {
            if (0 == memcmp(ABC_BUF_PTR(LRA), ABC_BUF_PTR(pKeys->LRA), ABC_BUF_SIZE(LRA)))
            {
                *pbValid = true;
            }
        }
    }
    else
    {
        // we will need to attempt to decrypt ELP2 in order to determine whether we have the right LRA
        // ELP2.json <- LP2 encrypted with recovery key (LRA2)

        // create our LRA2 = Scrypt(L + RA, SNRP3)
        ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pKeys->pSNRP3, &LRA2, pError));

        // attempt to decode ELP2
        ABC_CHECK_RET(ABC_AccountGetSyncDirName(szUserName, &szAccountSyncDir, pError));
        szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
        sprintf(szFilename, "%s/%s", szAccountSyncDir, ACCOUNT_ELP2_FILENAME);
        tABC_CC CC_Decrypt = ABC_CryptoDecryptJSONFile(szFilename, LRA2, &LP2, pError);

        // check the results
        if (ABC_CC_Ok == CC_Decrypt)
        {
            *pbValid = true;
        }
        else if (ABC_CC_DecryptFailure != CC_Decrypt)
        {
            // this was an error other than just a bad key so we need to treat this like an error
            cc = CC_Decrypt;
            goto exit;
        }
        else
        {
            // clear the error because we know why it failed and we will set that in the pbValid
            ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
        }

        // if we were successful, save our keys in the cache since we spent the time to create them
        if (*pbValid == true)
        {
            ABC_BUF_SET(pKeys->LRA, LRA);
            ABC_BUF_CLEAR(LRA); // so we don't free as we exit
            ABC_BUF_SET(pKeys->LRA2, LRA2);
            ABC_BUF_CLEAR(LRA2); // so we don't free as we exit
            ABC_BUF_SET(pKeys->LP2, LP2);
            ABC_BUF_CLEAR(LP2); // so we don't free as we exit
        }
    }

exit:
    ABC_BUF_FREE(LRA);
    ABC_BUF_FREE(LRA2);
    ABC_BUF_FREE(LP2);
    if (szAccountSyncDir) free(szAccountSyncDir);
    if (szFilename) free(szFilename);

    return cc;
}

/**
 * Gets the recovery question choices from the server.
 *
 * This function gets the recovery question choices from the server in
 * the form of a JSON object which is an array of the choices
 *
 * @param L1            Login hash for the account
 * @param ppJSON_Q      Pointer to store allocated json object
 *                      (it is the responsibility of the caller to free the ref)
 */
static
tABC_CC ABC_AccountServerGetQuestions(tABC_U08Buf L1, json_t **ppJSON_Q, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t  *pJSON_Root     = NULL;
    char    *szURL          = NULL;
    char    *szPost         = NULL;
    char    *szL1_Base64    = NULL;
    char    *szResults      = NULL;

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL(ppJSON_Q);
    // create the URL
    szURL = malloc(ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_GET_QUESTIONS_PATH);

    // create base64 versions of L1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{ss}", ABC_SERVER_JSON_L1_FIELD, szL1_Base64);
    szPost = json_dumps(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %s", szURL, szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %s", szResults);

    // decode the result
    json_t *pJSON_Value = NULL;
    json_error_t error;
    pJSON_Root = json_loads(szResults, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing server JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // get the status code
    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_STATUS_CODE_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_number(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON status code");
    int statusCode = (int) json_integer_value(pJSON_Value);

    // if there was a failure
    if (ABC_Server_Code_Success != statusCode)
    {
        if (ABC_Server_Code_NoAccount == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "Account does not exist on server");
        }
        else
        {
            // get the message
            pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_MESSAGE_FIELD);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_DebugLog("Server message: %s", json_string_value(pJSON_Value));
            ABC_RET_ERROR(ABC_CC_ServerError, json_string_value(pJSON_Value));
        }
    }

    // get the questions
    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_RESULTS_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_array(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON question results");
    *ppJSON_Q = pJSON_Value;
    json_incref(*ppJSON_Q);

exit:

    if (pJSON_Root)     json_decref(pJSON_Root);
    if (szURL)          free(szURL);
    if (szPost)         free(szPost);
    if (szL1_Base64)    free(szL1_Base64);
    if (szResults)      free(szResults);
    
    return cc;
}

/**
 * Gets the recovery question choices from the server and saves them
 * to local storage.
 *
 * @param szUserName UserName for a valid account to retrieve questions
 */
static
tABC_CC ABC_AccountUpdateQuestionChoices(const char *szUserName, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Root = NULL;
    json_t *pJSON_Q    = NULL;
    char   *szRootDir  = NULL;
    char   *szFilename = NULL;
    char   *szJSON     = NULL;


    ABC_CHECK_NULL(szUserName);

    // get L1 from the key cache
    tABC_U08Buf L1;
    ABC_CHECK_RET(ABC_AccountGetKey(szUserName, NULL, ABC_AccountKey_L1, &L1, pError));

    // get the questions from the server
    ABC_CHECK_RET(ABC_AccountServerGetQuestions(L1, &pJSON_Q, pError));

    // create the json object that will be our questions
    pJSON_Root = json_object();

    // set our final json for the array element
    json_object_set(pJSON_Root, JSON_ACCT_QUESTIONS_FIELD, pJSON_Q);

    // create the filename for the question json
    ABC_CHECK_RET(ABC_AccountGetRootDir(&szRootDir, pError));
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", szRootDir, ACCOUNT_QUESTIONS_FILENAME);

    // get the JSON for the file
    szJSON = json_dumps(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

    // write the file
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szJSON, pError));

exit:

    if (pJSON_Root)     json_decref(pJSON_Root);
    if (pJSON_Q)        json_decref(pJSON_Q);
    if (szRootDir)      free(szRootDir);
    if (szFilename)     free(szFilename);
    if (szJSON)         free(szJSON);

    return cc;
}

// allocates the account questions info structure
tABC_CC ABC_AccountQuestionsInfoAlloc(tABC_AccountQuestionsInfo **ppAccountQuestionsInfo,
                                   const char *szUserName,
                                   tABC_Request_Callback fRequestCallback,
                                   void *pData,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(ppAccountQuestionsInfo);
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(fRequestCallback);

    tABC_AccountQuestionsInfo *pAccountQuestionsInfo = (tABC_AccountQuestionsInfo *) calloc(1, sizeof(tABC_AccountQuestionsInfo));

    pAccountQuestionsInfo->szUserName = strdup(szUserName);

    pAccountQuestionsInfo->fRequestCallback = fRequestCallback;

    pAccountQuestionsInfo->pData = pData;

    *ppAccountQuestionsInfo = pAccountQuestionsInfo;

exit:

    return cc;
}

// frees the account signin info structure
void ABC_AccountQuestionsInfoFree(tABC_AccountQuestionsInfo *pAccountQuestionsInfo)
{
    if (pAccountQuestionsInfo)
    {
        if (pAccountQuestionsInfo->szUserName)
        {
            memset(pAccountQuestionsInfo->szUserName, 0, strlen(pAccountQuestionsInfo->szUserName));

            free((void *)pAccountQuestionsInfo->szUserName);
        }

        free(pAccountQuestionsInfo);
    }
}

/**
 * Gets the recovery question choices Assumes it is running in a thread.
 *
 * This function gets the recovery question choices.
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to create an account (should be a tABC_AccountQuestionsInfo)
 */
void *ABC_AccountGetQuestionChoicesThreaded(void *pData)
{
    tABC_AccountQuestionsInfo *pInfo = (tABC_AccountQuestionsInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;

        results.requestType = ABC_RequestType_GetQuestionChoices;

        results.bSuccess = false;

        // create the account
        tABC_CC CC = ABC_AccountGetQuestionChoices(pInfo, (tABC_QuestionChoices **) &(results.pRetData), &(results.errorInfo));
        results.errorInfo.code = CC;

        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);

        // it is our responsibility to free the info struct
        ABC_AccountQuestionsInfoFree(pInfo);
    }
    
    return NULL;
}

/**
 * Gets the recovery question chioces with the given info.
 *
 * @param pInfo             Pointer to recovery question chioces information
 * @param ppQuestionChoices Pointer to hold allocated pointer to recovery question chioces
 */
static
tABC_CC ABC_AccountGetQuestionChoices(tABC_AccountQuestionsInfo *pInfo,
                                      tABC_QuestionChoices      **ppQuestionChoices,
                                      tABC_Error                *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szRootDir = NULL;
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;
    json_t *pJSON_Value = NULL;
    tABC_QuestionChoices *pQuestionChoices = NULL;

    ABC_CHECK_NULL(pInfo);
    ABC_CHECK_NULL(pInfo->szUserName);
    ABC_CHECK_NULL(ppQuestionChoices);

    ABC_CHECK_RET(ABC_AccountCheckValidUser(pInfo->szUserName, pError));

    // create the filename for the question json
    ABC_CHECK_RET(ABC_AccountGetRootDir(&szRootDir, pError));
    szFilename = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", szRootDir, ACCOUNT_QUESTIONS_FILENAME);

    // if the file doesn't exist
    if (false == ABC_FileIOFileExist(szFilename))
    {
        // get an update from the server
        ABC_CHECK_RET(ABC_AccountUpdateQuestionChoices(pInfo->szUserName, pError));
    }

    // read in the recovery question choices json object
    ABC_CHECK_RET(ABC_FileIOReadFileObject(szFilename, &pJSON_Root, true, pError));

    // get the questions array field
    pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_QUESTIONS_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_array(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON array value for recovery questions");

    // get the number of elements in the array
    unsigned int count = (unsigned int) json_array_size(pJSON_Value);
    if (count > 0)
    {
        // allocate the data
        pQuestionChoices = calloc(1, sizeof(tABC_QuestionChoices));
        pQuestionChoices->numChoices = count;
        pQuestionChoices->aChoices = calloc(1, sizeof(tABC_QuestionChoice *) * count);

        for (int i = 0; i < count; i++)
        {
            json_t *pJSON_Elem = json_array_get(pJSON_Value, i);
            ABC_CHECK_ASSERT((pJSON_Elem && json_is_object(pJSON_Elem)), ABC_CC_JSONError, "Error parsing JSON element value for recovery questions");

            // allocate this element
            pQuestionChoices->aChoices[i] = calloc(1, sizeof(tABC_QuestionChoice));

            // get the category
            json_t *pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_CATEGORY_FIELD);
            ABC_CHECK_ASSERT((pJSON_Obj && json_is_string(pJSON_Obj)), ABC_CC_JSONError, "Error parsing JSON category value for recovery questions");
            pQuestionChoices->aChoices[i]->szCategory = strdup(json_string_value(pJSON_Obj));

            // get the question
            pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_QUESTION_FIELD);
            ABC_CHECK_ASSERT((pJSON_Obj && json_is_string(pJSON_Obj)), ABC_CC_JSONError, "Error parsing JSON question value for recovery questions");
            pQuestionChoices->aChoices[i]->szQuestion = strdup(json_string_value(pJSON_Obj));

            // get the min length
            pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_MIN_LENGTH_FIELD);
            ABC_CHECK_ASSERT((pJSON_Obj && json_is_integer(pJSON_Obj)), ABC_CC_JSONError, "Error parsing JSON min length value for recovery questions");
            pQuestionChoices->aChoices[i]->minAnswerLength = (unsigned int) json_integer_value(pJSON_Obj);
        }

        // assign final data
        *ppQuestionChoices = pQuestionChoices;
        pQuestionChoices = NULL; // so we don't free it below

    }
    else
    {
        ABC_RET_ERROR(ABC_CC_JSONError, "No questions in the recovery question choices file")
    }

exit:
    if (szRootDir)  free(szRootDir);
    if (szFilename) free(szFilename);
    if (pJSON_Root) json_decref(pJSON_Root);
    if (pQuestionChoices) ABC_AccountFreeQuestionChoices(pQuestionChoices);
    
    return cc;
}


/**
 * Free question choices.
 *
 * This function frees the question choices given
 *
 * @param pQuestionChoices  Pointer to question choices to free.
 */
void ABC_AccountFreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices)
{
    if (pQuestionChoices != NULL)
    {
        if ((pQuestionChoices->aChoices != NULL) && (pQuestionChoices->numChoices > 0))
        {
            for (int i = 0; i < pQuestionChoices->numChoices; i++)
            {
                tABC_QuestionChoice *pChoice = pQuestionChoices->aChoices[i];

                if (pChoice)
                {
                    if (pChoice->szQuestion)
                    {
                        memset(pChoice->szQuestion, 0, strlen(pChoice->szQuestion));
                        free(pChoice->szQuestion);
                    }

                    if (pChoice->szCategory)
                    {
                        memset(pChoice->szCategory, 0, strlen(pChoice->szCategory));
                        free(pChoice->szCategory);
                    }
                }
            }

            memset(pQuestionChoices->aChoices, 0, sizeof(tABC_QuestionChoice *) * pQuestionChoices->numChoices);
            free(pQuestionChoices->aChoices);
        }

        memset(pQuestionChoices, 0, sizeof(tABC_QuestionChoices));
        free(pQuestionChoices);
    }
}


