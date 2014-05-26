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
#include <time.h>
#include <unistd.h>
#include <jansson.h>
#include "ABC_Account.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Crypto.h"
#include "ABC_URL.h"
#include "ABC_Debug.h"
#include "ABC_ServerDefs.h"
#include "ABC_Wallet.h"
#include "ABC_Mutex.h"

#define ACCOUNT_MAX                             1024  // maximum number of accounts
#define ACCOUNT_DIR                             "Accounts"
#define ACCOUNT_SYNC_DIR                        "sync"
#define ACCOUNT_FOLDER_PREFIX                   "Account_"
#define ACCOUNT_NAME_FILENAME                   "User_Name.json"
#define ACCOUNT_EPIN_FILENAME                   "EPIN.json"
#define ACCOUNT_CARE_PACKAGE_FILENAME           "Care_Package.json"
#define ACCOUNT_WALLETS_FILENAME                "Wallets.json"
#define ACCOUNT_CATEGORIES_FILENAME             "Categories.json"
#define ACCOUNT_ELP2_FILENAME                   "ELP2.json"
#define ACCOUNT_ELRA2_FILENAME                  "ELRA2.json"
#define ACCOUNT_QUESTIONS_FILENAME              "Questions.json"
#define ACCOUNT_SETTINGS_FILENAME               "Settings.json"
#define ACCOUNT_INFO_FILENAME                   "Info.json"

#define JSON_ACCT_USERNAME_FIELD                "userName"
#define JSON_ACCT_PIN_FIELD                     "PIN"
#define JSON_ACCT_QUESTIONS_FIELD               "questions"
#define JSON_ACCT_WALLETS_FIELD                 "wallets"
#define JSON_ACCT_CATEGORIES_FIELD              "categories"
#define JSON_ACCT_ERQ_FIELD                     "ERQ"
#define JSON_ACCT_SNRP_FIELD_PREFIX             "SNRP"
#define JSON_ACCT_QUESTIONS_FIELD               "questions"

#define JSON_ACCT_FIRST_NAME_FIELD              "firstName"
#define JSON_ACCT_LAST_NAME_FIELD               "lastName"
#define JSON_ACCT_NICKNAME_FIELD                "nickname"
#define JSON_ACCT_NAME_ON_PAYMENTS_FIELD        "nameOnPayments"
#define JSON_ACCT_MINUTES_AUTO_LOGOUT_FIELD     "minutesAutoLogout"
#define JSON_ACCT_LANGUAGE_FIELD                "language"
#define JSON_ACCT_NUM_CURRENCY_FIELD            "numCurrency"
#define JSON_ACCT_EX_RATE_SOURCES_FIELD         "exchangeRateSources"
#define JSON_ACCT_EX_RATE_SOURCE_FIELD          "exchangeRateSource"
#define JSON_ACCT_BITCOIN_DENOMINATION_FIELD    "bitcoinDenomination"
#define JSON_ACCT_LABEL_FIELD                   "label"
#define JSON_ACCT_SATOSHI_FIELD                 "satoshi"
#define JSON_ACCT_ADVANCED_FEATURES_FIELD       "advancedFeatures"

#define JSON_INFO_MINERS_FEES_FIELD             "minersFees"
#define JSON_INFO_MINERS_FEE_SATOSHI_FIELD      "feeSatoshi"
#define JSON_INFO_MINERS_FEE_TX_SIZE_FIELD      "txSizeBytes"
#define JSON_INFO_AIRBITZ_FEES_FIELD            "feesAirBitz"
#define JSON_INFO_AIRBITZ_FEE_PERCENTAGE_FIELD  "percentage"
#define JSON_INFO_AIRBITZ_FEE_MAX_SATOSHI_FIELD "maxSatoshi"
#define JSON_INFO_AIRBITZ_FEE_MIN_SATOSHI_FIELD "minSatoshi"
#define JSON_INFO_AIRBITZ_FEE_ADDRESS_FIELD     "address"
#define JSON_INFO_OBELISK_SERVERS_FIELD         "obeliskServers"

#define ACCOUNT_ACCEPTABLE_INFO_FILE_AGE_SECS   (7 * 24 * 60 * 60) // how many seconds old can the info file before it should be updated

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

static tABC_CC ABC_AccountServerCreate(tABC_U08Buf L1, tABC_U08Buf P1, tABC_Error *pError);
static tABC_CC ABC_AccountServerChangePassword(tABC_U08Buf L1, tABC_U08Buf oldP1, tABC_U08Buf LRA1, tABC_U08Buf newP1, tABC_Error *pError);
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
static tABC_CC ABC_AccountGetGeneralInfoFilename(char **pszFilename, tABC_Error *pError);
static tABC_CC ABC_AccountGetSettingsFilename(const char *szUserName, char **pszFilename, tABC_Error *pError);
static tABC_CC ABC_AccountCreateDefaultSettings(tABC_AccountSettings **ppSettings, tABC_Error *pError);
static tABC_CC ABC_AccountLoadSettingsEnc(const char *szUserName, tABC_U08Buf Key, tABC_AccountSettings **ppSettings, tABC_Error *pError);
static tABC_CC ABC_AccountSaveSettingsEnc(const char *szUserName, tABC_U08Buf Key, tABC_AccountSettings *pSettings, tABC_Error *pError);
static tABC_CC ABC_AccountMutexLock(tABC_Error *pError);
static tABC_CC ABC_AccountMutexUnlock(tABC_Error *pError);

/**
 * Allocates and fills in an account request structure with the info given.
 *
 * @param ppAccountRequestInfo      Pointer to store allocated request info
 * @param requestType               Type of request this is being used for
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account (can be NULL for some requests)
 * @param szRecoveryQuestions       Recovery questions seperated by newlines (can be NULL for some requests)
 * @param szRecoveryAnswers         Recovery answers seperated by newlines (can be NULL for some requests)
 * @param szPIN                     PIN number for the account (can be NULL for some requests)
 * @param szNewPassword             New password for the account (for change password requests)
 * @param fRequestCallback          The function that will be called when the account create process has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_AccountRequestInfoAlloc(tABC_AccountRequestInfo **ppAccountRequestInfo,
                                    tABC_RequestType requestType,
                                    const char *szUserName,
                                    const char *szPassword,
                                    const char *szRecoveryQuestions,
                                    const char *szRecoveryAnswers,
                                    const char *szPIN,
                                    const char *szNewPassword,
                                    tABC_Request_Callback fRequestCallback,
                                    void *pData,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(ppAccountRequestInfo);
    ABC_CHECK_NULL(szUserName);

    tABC_AccountRequestInfo *pAccountRequestInfo = NULL;
    ABC_ALLOC(pAccountRequestInfo, sizeof(tABC_AccountRequestInfo));

    pAccountRequestInfo->requestType = requestType;

    ABC_STRDUP(pAccountRequestInfo->szUserName, szUserName);

    if (NULL != szPassword)
    {
        ABC_STRDUP(pAccountRequestInfo->szPassword, szPassword);
    }

    if (NULL != szRecoveryQuestions)
    {
        ABC_STRDUP(pAccountRequestInfo->szRecoveryQuestions, szRecoveryQuestions);
    }

    if (NULL != szRecoveryAnswers)
    {
        ABC_STRDUP(pAccountRequestInfo->szRecoveryAnswers, szRecoveryAnswers);
    }

    if (NULL != szPIN)
    {
        ABC_STRDUP(pAccountRequestInfo->szPIN, szPIN);
    }

    if (NULL != szNewPassword)
    {
        ABC_STRDUP(pAccountRequestInfo->szNewPassword, szNewPassword);
    }

    pAccountRequestInfo->pData = pData;

    pAccountRequestInfo->fRequestCallback = fRequestCallback;

    *ppAccountRequestInfo = pAccountRequestInfo;

exit:

    return cc;
}

/**
 * Frees the account creation info structure
 */
void ABC_AccountRequestInfoFree(tABC_AccountRequestInfo *pAccountRequestInfo)
{
    if (pAccountRequestInfo)
    {
        ABC_FREE_STR(pAccountRequestInfo->szUserName);

        ABC_FREE_STR(pAccountRequestInfo->szPassword);

        ABC_FREE_STR(pAccountRequestInfo->szRecoveryQuestions);

        ABC_FREE_STR(pAccountRequestInfo->szRecoveryAnswers);

        ABC_FREE_STR(pAccountRequestInfo->szPIN);

        ABC_FREE_STR(pAccountRequestInfo->szNewPassword);

        ABC_CLEAR_FREE(pAccountRequestInfo, sizeof(tABC_AccountRequestInfo));
    }
}

/**
 * Performs the request specified. Assumes it is running in a thread.
 *
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to create an account (should be a tABC_AccountCreateInfo)
 */
void *ABC_AccountRequestThreaded(void *pData)
{
    tABC_AccountRequestInfo *pInfo = (tABC_AccountRequestInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;

        results.requestType = pInfo->requestType;

        results.bSuccess = false;

        tABC_CC CC = ABC_CC_Error;

        // perform the appropriate request
        if (ABC_RequestType_CreateAccount == pInfo->requestType)
        {
            // create the account
            CC = ABC_AccountCreate(pInfo, &(results.errorInfo));
        }
        else if (ABC_RequestType_AccountSignIn == pInfo->requestType)
        {
            // sign-in
            CC = ABC_AccountSignIn(pInfo, &(results.errorInfo));
        }
        else if (ABC_RequestType_GetQuestionChoices == pInfo->requestType)
        {
            // get the recovery question choices
            CC = ABC_AccountGetQuestionChoices(pInfo, (tABC_QuestionChoices **) &(results.pRetData), &(results.errorInfo));
        }
        else if (ABC_RequestType_SetAccountRecoveryQuestions == pInfo->requestType)
        {
            // set the recovery information
            CC = ABC_AccountSetRecovery(pInfo, &(results.errorInfo));
        }
        else if (ABC_RequestType_ChangePassword == pInfo->requestType)
        {
            // change the password
            CC = ABC_AccountChangePassword(pInfo, &(results.errorInfo));
        }


        results.errorInfo.code = CC;

        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);

        // it is our responsibility to free the info struct
        ABC_AccountRequestInfoFree(pInfo);
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

/**
 * Signs into an account
 * This cache's the keys for an account
 */
tABC_CC ABC_AccountSignIn(tABC_AccountRequestInfo *pInfo,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(pInfo);

    // check the credentials
    ABC_CHECK_RET(ABC_AccountCheckCredentials(pInfo->szUserName, pInfo->szPassword, pError));

    // take this non-blocking opportunity to update the info from the server if needed
    ABC_CHECK_RET(ABC_AccountServerUpdateGeneralInfo(pError));

exit:

    return cc;
}

/**
 * Create and account
 */
tABC_CC ABC_AccountCreate(tABC_AccountRequestInfo *pInfo,
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

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
    ABC_CHECK_NULL(pInfo);

    int AccountNum = 0;

    // check locally that the account name is available
    ABC_CHECK_RET(ABC_AccountNumForUser(pInfo->szUserName, &AccountNum, pError));
    if (AccountNum >= 0)
    {
        ABC_RET_ERROR(ABC_CC_AccountAlreadyExists, "Account already exists");
    }

    // create an account keys struct
    ABC_ALLOC(pKeys, sizeof(tAccountKeys));
    ABC_STRDUP(pKeys->szUserName, pInfo->szUserName);
    ABC_STRDUP(pKeys->szPassword, pInfo->szPassword);
    ABC_STRDUP(pKeys->szPIN, pInfo->szPIN);

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
    ABC_ALLOC(szAccountDir, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, pKeys->accountNum, pError));
    ABC_CHECK_RET(ABC_FileIOCreateDir(szAccountDir, pError));

    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);

    // create the name file data and write the file
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(pKeys->szUserName, JSON_ACCT_USERNAME_FIELD, &szJSON, pError));
    sprintf(szFilename, "%s/%s", szAccountDir, ACCOUNT_NAME_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szJSON, pError));
    ABC_FREE_STR(szJSON);
    szJSON = NULL;

    // create the PIN JSON
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(pKeys->szPIN, JSON_ACCT_PIN_FIELD, &szJSON, pError));
    tABC_U08Buf PIN = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(PIN, (unsigned char *)szJSON, strlen(szJSON) + 1);

    // EPIN = AES256(PIN, LP2)
    ABC_CHECK_RET(ABC_CryptoEncryptJSONString(PIN, pKeys->LP2, ABC_CryptoType_AES256, &szEPIN_JSON, pError));
    sprintf(szFilename, "%s/%s", szAccountDir, ACCOUNT_EPIN_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szEPIN_JSON, pError));
    ABC_FREE_STR(szJSON);
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

    // also take this non-blocking opportunity to update the info from the server if needed
    ABC_CHECK_RET(ABC_AccountServerUpdateGeneralInfo(pError));

exit:
    if (pKeys)
    {
        ABC_AccountFreeAccountKeys(pKeys);
        ABC_CLEAR_FREE(pKeys, sizeof(tAccountKeys));
    }
    if (pJSON_SNRP2)        json_decref(pJSON_SNRP2);
    if (pJSON_SNRP3)        json_decref(pJSON_SNRP3);
    if (pJSON_SNRP4)        json_decref(pJSON_SNRP4);
    ABC_FREE_STR(szCarePackage_JSON);
    ABC_FREE_STR(szEPIN_JSON);
    ABC_FREE_STR(szJSON);
    ABC_FREE_STR(szAccountDir);
    ABC_FREE_STR(szFilename);

    ABC_AccountMutexUnlock(NULL);
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
    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_ACCOUNT_CREATE_PATH);

    // create base64 versions of L1 and P1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(P1, &szP1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{ssss}", ABC_SERVER_JSON_L1_FIELD, szL1_Base64, ABC_SERVER_JSON_P1_FIELD, szP1_Base64);
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
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
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szResults);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szL1_Base64);
    ABC_FREE_STR(szP1_Base64);
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
tABC_CC ABC_AccountSetRecovery(tABC_AccountRequestInfo *pInfo,
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

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
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
    //ABC_UtilHexDumpBuf("LP2", pKeys->LP2);

    // ELRA2 = AES256(LRA2, LP2)
    ABC_CHECK_RET(ABC_CryptoEncryptJSONString(pKeys->LRA2, pKeys->LP2, ABC_CryptoType_AES256, &szELRA2_JSON, pError));


    // write out the files

    // create the main account directory
    ABC_ALLOC(szAccountDir, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, pKeys->accountNum, pError));

    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);

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
    ABC_FREE_STR(szCarePackage_JSON);
    ABC_FREE_STR(szELP2_JSON);
    ABC_FREE_STR(szELRA2_JSON);
    ABC_FREE_STR(szAccountDir);
    ABC_FREE_STR(szFilename);

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Change password for an account
 *
 * This function sets the password recovery informatin for the account.
 * This includes sending a new care package to the server
 *
 * @param pInfo     Pointer to password change information data
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_AccountChangePassword(tABC_AccountRequestInfo *pInfo,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tAccountKeys *pKeys = NULL;
    tABC_U08Buf oldLP2 = ABC_BUF_NULL;
    tABC_U08Buf LRA2 = ABC_BUF_NULL;
    tABC_U08Buf LRA = ABC_BUF_NULL;
    tABC_U08Buf LRA1 = ABC_BUF_NULL;
    tABC_U08Buf oldP1 = ABC_BUF_NULL;
    tABC_U08Buf SettingsData = ABC_BUF_NULL;
    char *szAccountDir = NULL;
    char *szFilename = NULL;
    char *szSettingsFilename = NULL;
    char *szJSON = NULL;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
    ABC_CHECK_NULL(pInfo);
    ABC_CHECK_NULL(pInfo->szUserName);
    ABC_CHECK_NULL(pInfo->szNewPassword);

    // get the account directory and set up for creating needed filenames
    ABC_CHECK_RET(ABC_AccountGetDirName(pInfo->szUserName, &szAccountDir, pError));
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);

    // get the keys for this user (note: password can be NULL for this call)
    ABC_CHECK_RET(ABC_AccountCacheKeys(pInfo->szUserName, pInfo->szPassword, &pKeys, pError));

    // we need to obtain the original LP2 and LRA2
    if (pInfo->szPassword != NULL)
    {
        // we had the password so we should have the LP2 key
        ABC_CHECK_ASSERT(NULL != ABC_BUF_PTR(pKeys->LP2), ABC_CC_Error, "Expected to find LP2 in key cache");
        ABC_BUF_DUP(oldLP2, pKeys->LP2);

        // if we don't yet have LRA2
        if (ABC_BUF_PTR(pKeys->LRA2) == NULL)
        {
            // get the LRA2 by decrypting ELRA2
            sprintf(szFilename, "%s/%s/%s", szAccountDir, ACCOUNT_SYNC_DIR, ACCOUNT_ELRA2_FILENAME);
            bool bExists = false;
            ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
            if (true == bExists)
            {
                ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, pKeys->LP2, &(pKeys->LRA2), pError));
            }
        }
        ABC_BUF_DUP(LRA2, pKeys->LRA2);

        // create the old P1 for use in server auth -> P1 = Scrypt(P, SNRP1)
        ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->P, pKeys->pSNRP1, &oldP1, pError));
    }
    else
    {
        // we have the recovery questions so we can make the LRA2

        // LRA = L + RA
        ABC_BUF_DUP(LRA, pKeys->L);
        ABC_BUF_APPEND_PTR(LRA, pInfo->szRecoveryAnswers, strlen(pInfo->szRecoveryAnswers));

        // LRA2 = Scrypt(LRA, SNRP3)
        ABC_CHECK_ASSERT(NULL != pKeys->pSNRP3, ABC_CC_Error, "Expected to find SNRP3 in key cache");
        ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pKeys->pSNRP3, &LRA2, pError));

        // get the LP2 by decrypting ELP2 with LRA2
        sprintf(szFilename, "%s/%s/%s", szAccountDir, ACCOUNT_SYNC_DIR, ACCOUNT_ELP2_FILENAME);
        ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, LRA2, &oldLP2, pError));

        // create LRA1 as it will be needed for server communication later
        ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pKeys->pSNRP1, &LRA1, pError));
    }

    // we now have oldLP2 and oldLRA2

    // time to set the new data for this account

    // set new PIN
    ABC_FREE_STR(pKeys->szPIN);
    ABC_STRDUP(pKeys->szPIN, pInfo->szPIN);

    // set new password
    ABC_FREE_STR(pKeys->szPassword);
    ABC_STRDUP(pKeys->szPassword, pInfo->szNewPassword);

    // set new P
    ABC_BUF_FREE(pKeys->P);
    ABC_BUF_DUP_PTR(pKeys->P, pKeys->szPassword, strlen(pKeys->szPassword));

    // set new P1
    ABC_BUF_FREE(pKeys->P1);
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->P, pKeys->pSNRP1, &(pKeys->P1), pError));

    // set new LP = L + P
    ABC_BUF_FREE(pKeys->LP);
    ABC_BUF_DUP(pKeys->LP, pKeys->L);
    ABC_BUF_APPEND(pKeys->LP, pKeys->P);

    // set new LP2 = Scrypt(L + P, SNRP2)
    ABC_BUF_FREE(pKeys->LP2);
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->LP, pKeys->pSNRP2, &(pKeys->LP2), pError));

    // we'll need L1 for server communication L1 = Scrypt(L, SNRP1)
    if (ABC_BUF_PTR(pKeys->L1) == NULL)
    {
        ABC_CHECK_RET(ABC_CryptoScryptSNRP(pKeys->L, pKeys->pSNRP1, &(pKeys->L1), pError));
    }

    // server change password - Server will need L1, (P1 or LRA1) and new_P1
    ABC_CHECK_RET(ABC_AccountServerChangePassword(pKeys->L1, oldP1, LRA1, pKeys->P1, pError));

    // change all the wallet keys - re-encrypted them with new LP2
    ABC_CHECK_RET(ABC_WalletChangeEMKsForAccount(pInfo->szUserName, oldLP2, pKeys->LP2, pError));

    if (ABC_BUF_PTR(LRA2) != NULL)
    {
        // write out new ELP2.json <- LP2 encrypted with recovery key (LRA2)
        sprintf(szFilename, "%s/%s/%s", szAccountDir, ACCOUNT_SYNC_DIR, ACCOUNT_ELP2_FILENAME);
        ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(pKeys->LP2, LRA2, ABC_CryptoType_AES256, szFilename, pError));

        // write out new ELRA2.json <- LRA2 encrypted with LP2 (L+P,S2)
        sprintf(szFilename, "%s/%s/%s", szAccountDir, ACCOUNT_SYNC_DIR, ACCOUNT_ELRA2_FILENAME);
        ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(LRA2, pKeys->LP2, ABC_CryptoType_AES256, szFilename, pError));
    }

    // re-encrypt the settings

    ABC_CHECK_RET(ABC_AccountGetSettingsFilename(pInfo->szUserName, &szSettingsFilename, pError));
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szSettingsFilename, &bExists, pError));
    if (true == bExists)
    {
        // load them using the old key
        ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szSettingsFilename, oldLP2, &SettingsData, pError));

        // save them using the new key
        ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(SettingsData, pKeys->LP2, ABC_CryptoType_AES256, szSettingsFilename, pError));
    }

    // the keys for the account have all been updated so other functions can now be called that use them

    // set the new PIN
    ABC_CHECK_RET(ABC_AccountSetPIN(pInfo->szUserName, pInfo->szNewPassword, pInfo->szPIN, pError));

exit:
    ABC_BUF_FREE(oldLP2);
    ABC_BUF_FREE(LRA2);
    ABC_BUF_FREE(LRA);
    ABC_BUF_FREE(LRA1);
    ABC_BUF_FREE(oldP1);
    ABC_BUF_FREE(SettingsData);
    ABC_FREE_STR(szAccountDir);
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szJSON);
    ABC_FREE_STR(szSettingsFilename);
    if (cc != ABC_CC_Ok) ABC_AccountClearKeyCache(NULL);

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Changes the password for an account on the server.
 *
 * This function sends information to the server to change the password for an account.
 * Either the old P1 or LRA1 can be used for authentication.
 *
 * @param L1    Login hash for the account
 * @param oldP1 Old password hash for the account (if this is empty, LRA1 is used instead)
 * @param LRA1  LRA1 for the account (used if oldP1 is empty)
 */
static
tABC_CC ABC_AccountServerChangePassword(tABC_U08Buf L1, tABC_U08Buf oldP1, tABC_U08Buf LRA1, tABC_U08Buf newP1, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szURL     = NULL;
    char *szResults = NULL;
    char *szPost    = NULL;
    char *szL1_Base64 = NULL;
    char *szOldP1_Base64 = NULL;
    char *szNewP1_Base64 = NULL;
    char *szAuth_Base64 = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL_BUF(newP1);

    // create the URL
    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_CHANGE_PASSWORD_PATH);

    // create base64 versions of L1 and newP1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(newP1, &szNewP1_Base64, pError));

    // create the post data
    if (ABC_BUF_PTR(oldP1) != NULL)
    {
        ABC_CHECK_RET(ABC_CryptoBase64Encode(oldP1, &szAuth_Base64, pError));
        pJSON_Root = json_pack("{ssssss}",
                               ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                               ABC_SERVER_JSON_P1_FIELD, szAuth_Base64,
                               ABC_SERVER_JSON_NEW_P1_FIELD, szNewP1_Base64);
    }
    else
    {
        ABC_CHECK_ASSERT(NULL != ABC_BUF_PTR(LRA1), ABC_CC_Error, "LRA1 missing for server password change auth");
        ABC_CHECK_RET(ABC_CryptoBase64Encode(LRA1, &szAuth_Base64, pError));
        pJSON_Root = json_pack("{ssssss}",
                               ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                               ABC_SERVER_JSON_LRA1_FIELD, szAuth_Base64,
                               ABC_SERVER_JSON_NEW_P1_FIELD, szNewP1_Base64);
    }
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
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
        // get the message
        pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_MESSAGE_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
        ABC_DebugLog("Server message: %s", json_string_value(pJSON_Value));
        ABC_RET_ERROR(ABC_CC_ServerError, json_string_value(pJSON_Value));
    }

exit:
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szResults);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szL1_Base64);
    ABC_FREE_STR(szOldP1_Base64);
    ABC_FREE_STR(szNewP1_Base64);
    ABC_FREE_STR(szAuth_Base64);
    if (pJSON_Root)     json_decref(pJSON_Root);

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
    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
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
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
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
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szResults);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szL1_Base64);
    ABC_FREE_STR(szP1_Base64);
    ABC_FREE_STR(szLRA1_Base64);
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

    ABC_ALLOC(szField, ABC_MAX_STRING_LENGTH);

    sprintf(szField, "%s%d", JSON_ACCT_SNRP_FIELD_PREFIX, 2);
    json_object_set(pJSON_Root, szField, (json_t *) pJSON_SNRP2);

    sprintf(szField, "%s%d", JSON_ACCT_SNRP_FIELD_PREFIX, 3);
    json_object_set(pJSON_Root, szField, (json_t *) pJSON_SNRP3);

    sprintf(szField, "%s%d", JSON_ACCT_SNRP_FIELD_PREFIX, 4);
    json_object_set(pJSON_Root, szField, (json_t *) pJSON_SNRP4);

    *pszJSON = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    ABC_FREE_STR(szField);

    return cc;
}

// loads the json care package for a given account number
// if the ERQ doesn't exist, ppJSON_ERQ is set to NULL

/**
 * Loads the json care package for a given account number
 *
 * The JSON objects for each argument will be assigned.
 * The function assumes any number of the arguments may be NULL,
 * in which case, they are not set.
 * It is also possible that there is no recovery questions, in which case
 * the ERQ will be set to NULL.
 *
 * @param AccountNum   Account number of the account of interest
 * @param ppJSON_ERQ   Pointer store ERQ JSON object (can be NULL) - caller expected to decref
 * @param ppJSON_SNRP2 Pointer store SNRP2 JSON object (can be NULL) - caller expected to decref
 * @param ppJSON_SNRP3 Pointer store SNRP3 JSON object (can be NULL) - caller expected to decref
 * @param ppJSON_SNRP4 Pointer store SNRP4 JSON object (can be NULL) - caller expected to decref
 * @param pError       A pointer to the location to store the error if there is one
 */
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

    // get the main account directory
    ABC_ALLOC(szAccountDir, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, AccountNum, pError));

    // create the name of the care package file
    ABC_ALLOC(szCarePackageFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szCarePackageFilename, "%s/%s", szAccountDir, ACCOUNT_CARE_PACKAGE_FILENAME);

    // load the care package
    ABC_CHECK_RET(ABC_FileIOReadFileStr(szCarePackageFilename, &szCarePackage_JSON, pError));

    // decode the json
    json_error_t error;
    pJSON_Root = json_loads(szCarePackage_JSON, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON care package");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON care package");

    // get the ERQ
    pJSON_ERQ = json_object_get(pJSON_Root, JSON_ACCT_ERQ_FIELD);
    //ABC_CHECK_ASSERT((pJSON_ERQ && json_is_object(pJSON_ERQ)), ABC_CC_JSONError, "Error parsing JSON care package - missing ERQ");

    ABC_ALLOC(szField, ABC_MAX_STRING_LENGTH);

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
        }
        else
        {
            *ppJSON_ERQ = NULL;
        }
    }
    if (ppJSON_SNRP2)
    {
        *ppJSON_SNRP2 = json_incref(pJSON_SNRP2);
    }
    if (ppJSON_SNRP3)
    {
        *ppJSON_SNRP3 = json_incref(pJSON_SNRP3);
    }
    if (ppJSON_SNRP4)
    {
        *ppJSON_SNRP4 = json_incref(pJSON_SNRP4);
    }

exit:
    if (pJSON_Root)             json_decref(pJSON_Root);
    ABC_FREE_STR(szAccountDir);
    ABC_FREE_STR(szCarePackageFilename);
    ABC_FREE_STR(szCarePackage_JSON);
    ABC_FREE_STR(szField);

    return cc;
}

/**
 * Creates a new sync directory and all the files needed for the given account
 * TODO: eventually this function needs the sync info
 */
static
tABC_CC ABC_AccountCreateSync(const char *szAccountsRootDir,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szDataJSON = NULL;
    char *szFilename = NULL;

    ABC_CHECK_NULL(szAccountsRootDir);

    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);

    // create the sync directory
    sprintf(szFilename, "%s/%s", szAccountsRootDir, ACCOUNT_SYNC_DIR);
    ABC_CHECK_RET(ABC_FileIOCreateDir(szFilename, pError));

    // create initial categories file with no entries
    ABC_CHECK_RET(ABC_AccountCreateListJSON(JSON_ACCT_CATEGORIES_FIELD, "", &szDataJSON, pError));
    sprintf(szFilename, "%s/%s/%s", szAccountsRootDir, ACCOUNT_SYNC_DIR, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szDataJSON, pError));
    ABC_FREE_STR(szDataJSON);
    szDataJSON = NULL;

    // TODO: create sync info in this directory

exit:
    ABC_FREE_STR(szDataJSON);
    ABC_FREE_STR(szFilename);

    return cc;
}

/**
 * Finds the next available account number (the number is just used for the directory name)
 */
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
    ABC_ALLOC(szAccountRoot, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(szAccountRoot, pError));

    // run through all the account names
    ABC_ALLOC(szAccountDir, ABC_FILEIO_MAX_PATH_LENGTH);
    int AccountNum;
    for (AccountNum = 0; AccountNum < ACCOUNT_MAX; AccountNum++)
    {
        ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, AccountNum, pError));
        bool bExists = false;
        ABC_CHECK_RET(ABC_FileIOFileExists(szAccountDir, &bExists, pError));
        if (true != bExists)
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
    ABC_FREE_STR(szAccountRoot);
    ABC_FREE_STR(szAccountDir);

    return cc;
}

/**
 * creates the account directory if needed
 */
static
tABC_CC ABC_AccountCreateRootDir(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szAccountRoot = NULL;

    // create the account directory string
    ABC_ALLOC(szAccountRoot, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(szAccountRoot, pError));

    // if it doesn't exist
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szAccountRoot, &bExists, pError));
    if (true != bExists)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szAccountRoot, pError));
    }

exit:
    ABC_FREE_STR(szAccountRoot);

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
    ABC_ALLOC(*pszRootDir, ABC_FILEIO_MAX_PATH_LENGTH);
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

    char *szFileIORootDir = NULL;

    ABC_CHECK_NULL(szRootDir);

    ABC_CHECK_RET(ABC_FileIOGetRootDir(&szFileIORootDir, pError));

    // create the account directory string
    sprintf(szRootDir, "%s/%s", szFileIORootDir, ACCOUNT_DIR);

exit:
    ABC_FREE_STR(szFileIORootDir);

    return cc;
}

/**
 * Gets the account directory for a given username
 *
 * @param pszDirName Location to store allocated pointer (must be free'd by caller)
 */
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
    ABC_ALLOC(szDirName, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szDirName, accountNum, pError));
    *pszDirName = szDirName;
    szDirName = NULL; // so we don't free it


exit:
    ABC_FREE_STR(szDirName);

    return cc;
}

/**
 * Gets the account sync directory for a given username
 *
 * @param pszDirName Location to store allocated pointer (must be free'd by caller)
 */
tABC_CC ABC_AccountGetSyncDirName(const char *szUserName,
                                  char **pszDirName,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szDirName = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(pszDirName);

    ABC_CHECK_RET(ABC_AccountGetDirName(szUserName, &szDirName, pError));

    ABC_ALLOC(*pszDirName, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszDirName, "%s/%s", szDirName, ACCOUNT_SYNC_DIR);

exit:
    ABC_FREE_STR(szDirName);

    return cc;
}

/*
 * Copies the account directory name into the string given
 */
static
tABC_CC ABC_AccountCopyAccountDirName(char *szAccountDir, int AccountNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szAccountRoot = NULL;

    ABC_CHECK_NULL(szAccountDir);

    // get the account root directory string
    ABC_ALLOC(szAccountRoot, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(szAccountRoot, pError));

    // create the account directory string
    sprintf(szAccountDir, "%s/%s%d", szAccountRoot, ACCOUNT_FOLDER_PREFIX, AccountNum);

exit:
    ABC_FREE_STR(szAccountRoot);

    return cc;
}

/**
 * creates the json for a list of items in a string seperated by newlines
 * for example:
 *   "A\nB\n"
 * becomes
 *  { "name" : [ "A", "B" ] }
 */
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
        ABC_STRDUP(szNewItems, szItems);
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

    *pszJSON = ABC_UtilStringFromJSONObject(jsonItems, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

exit:
    if (jsonItems)      json_decref(jsonItems);
    if (jsonItemArray)  json_decref(jsonItemArray);
    ABC_FREE_STR(szNewItems);

    return cc;
}


/*
 * returns the account number associated with the given user name
 * -1 is returned if the account does not exist
 */
static
tABC_CC ABC_AccountNumForUser(const char *szUserName, int *pAccountNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szCurUserName = NULL;
    char *szAccountRoot = NULL;
    tABC_FileIOList *pFileList = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(pAccountNum);

    // assume we didn't find it
    *pAccountNum = -1;

    // make sure the accounts directory is in place
    ABC_CHECK_RET(ABC_AccountCreateRootDir(pError));

    // get the account root directory string
    ABC_ALLOC(szAccountRoot, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(szAccountRoot, pError));

    // get all the files in this root

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
                ABC_FREE_STR(szCurUserName);
                szCurUserName = NULL;
            }
        }
    }


exit:
    ABC_FREE_STR(szCurUserName);
    ABC_FREE_STR(szAccountRoot);
    ABC_FileIOFreeFileList(pFileList);

    return cc;
}

/**
 * Gets the user name for the specified account number
 *
 * @param pszUserName Location to store allocated pointer (must be free'd by caller)
 */
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
    ABC_ALLOC(szAccountRoot, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_AccountCopyRootDirName(szAccountRoot, pError));

    // create the path to the account name file
    ABC_ALLOC(szAccountNamePath, ABC_FILEIO_MAX_PATH_LENGTH);
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

    ABC_STRDUP(*pszUserName, szUserName);

exit:
    if (root)               json_decref(root);
    ABC_FREE_STR(szAccountNameJSON);
    ABC_FREE_STR(szAccountRoot);
    ABC_FREE_STR(szAccountNamePath);

    return cc;
}

/**
 * Clears all the keys from the cache
 */
tABC_CC ABC_AccountClearKeyCache(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));

    if ((gAccountKeysCacheCount > 0) && (NULL != gaAccountKeysCacheArray))
    {
        for (int i = 0; i < gAccountKeysCacheCount; i++)
        {
            tAccountKeys *pAccountKeys = gaAccountKeysCacheArray[i];
            ABC_AccountFreeAccountKeys(pAccountKeys);
        }

        ABC_FREE(gaAccountKeysCacheArray);
        gAccountKeysCacheCount = 0;
    }

exit:

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Frees all the elements in the given AccountKeys struct
 */
static void ABC_AccountFreeAccountKeys(tAccountKeys *pAccountKeys)
{
    if (pAccountKeys)
    {
        ABC_FREE_STR(pAccountKeys->szUserName);

        ABC_FREE_STR(pAccountKeys->szPassword);

        ABC_FREE_STR(pAccountKeys->szPIN);

        ABC_CryptoFreeSNRP(&(pAccountKeys->pSNRP1));
        ABC_CryptoFreeSNRP(&(pAccountKeys->pSNRP2));
        ABC_CryptoFreeSNRP(&(pAccountKeys->pSNRP3));
        ABC_CryptoFreeSNRP(&(pAccountKeys->pSNRP4));

        ABC_BUF_FREE(pAccountKeys->L);
        ABC_BUF_FREE(pAccountKeys->L1);
        ABC_BUF_FREE(pAccountKeys->P);
        ABC_BUF_FREE(pAccountKeys->P1);
        ABC_BUF_FREE(pAccountKeys->LRA);
        ABC_BUF_FREE(pAccountKeys->LRA1);
        ABC_BUF_FREE(pAccountKeys->L2);
        ABC_BUF_FREE(pAccountKeys->RQ);
        ABC_BUF_FREE(pAccountKeys->LP);
        ABC_BUF_FREE(pAccountKeys->LP2);
        ABC_BUF_FREE(pAccountKeys->LRA2);
    }
}

/**
 * Adds the given AccountKey to the array of cached account keys
 */
static tABC_CC ABC_AccountAddToKeyCache(tAccountKeys *pAccountKeys, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
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
            ABC_ALLOC(gaAccountKeysCacheArray, sizeof(tAccountKeys *));
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

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Searches for a key in the cached by account name
 * if it is not found, the account keys will be set to NULL
 */
static tABC_CC ABC_AccountKeyFromCacheByName(const char *szUserName, tAccountKeys **ppAccountKeys, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
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

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Adds the given user to the key cache if it isn't already cached.
 * With or without a password, szUserName, L, SNRP1, SNRP2, SNRP3, SNRP4 keys are retrieved and added if they aren't already in the cache
 * If a password is given, szPassword, szPIN, P, LP2 keys are retrieved and the entry is added
 *  (the initial keys are added so the password can be verified while trying to decrypt EPIN)
 * If a pointer to hold the keys is given, then it is set to those keys
 */
static
tABC_CC ABC_AccountCacheKeys(const char *szUserName, const char *szPassword, tAccountKeys **ppKeys, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tAccountKeys *pKeys         = NULL;
    tAccountKeys *pFinalKeys    = NULL;
    char         *szFilename    = NULL;
    char         *szAccountDir  = NULL;
    json_t       *pJSON_SNRP2   = NULL;
    json_t       *pJSON_SNRP3   = NULL;
    json_t       *pJSON_SNRP4   = NULL;
    tABC_U08Buf  PIN_JSON       = ABC_BUF_NULL;
    json_t       *pJSON_Root    = NULL;
    tABC_U08Buf  P              = ABC_BUF_NULL;
    tABC_U08Buf  LP             = ABC_BUF_NULL;
    tABC_U08Buf  LP2            = ABC_BUF_NULL;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
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
            ABC_ALLOC(pKeys, sizeof(tAccountKeys));
            pKeys->accountNum = AccountNum;
            ABC_STRDUP(pKeys->szUserName, szUserName);

            ABC_CHECK_RET(ABC_AccountGetCarePackageObjects(AccountNum, NULL, &pJSON_SNRP2, &pJSON_SNRP3, &pJSON_SNRP4, pError));

            // SNRP's
            ABC_CHECK_RET(ABC_CryptoCreateSNRPForServer(&(pKeys->pSNRP1), pError));
            ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP2, &(pKeys->pSNRP2), pError));
            ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP3, &(pKeys->pSNRP3), pError));
            ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP4, &(pKeys->pSNRP4), pError));

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
            ABC_ALLOC(szAccountDir, ABC_FILEIO_MAX_PATH_LENGTH);
            ABC_CHECK_RET(ABC_AccountCopyAccountDirName(szAccountDir, pFinalKeys->accountNum, pError));
            ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
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
            ABC_STRDUP(pFinalKeys->szPassword, szPassword);
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
        ABC_CLEAR_FREE(pKeys, sizeof(tAccountKeys));
    }
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szAccountDir);
    if (pJSON_SNRP2)    json_decref(pJSON_SNRP2);
    if (pJSON_SNRP3)    json_decref(pJSON_SNRP3);
    if (pJSON_SNRP4)    json_decref(pJSON_SNRP4);
    if (pJSON_Root)     json_decref(pJSON_Root);
    ABC_BUF_FREE(PIN_JSON);
    ABC_BUF_FREE(P);
    ABC_BUF_FREE(LP);
    ABC_BUF_FREE(LP2);

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Retrieves the specified key from the key cache
 * if the account associated with the username and password is not currently in the cache, it is added
 */
tABC_CC ABC_AccountGetKey(const char *szUserName, const char *szPassword, tABC_AccountKey keyType, tABC_U08Buf *pKey, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tAccountKeys *pKeys = NULL;
    json_t       *pJSON_ERQ     = NULL;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
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

        case ABC_AccountKey_RQ:
            // RQ - if ERQ available
            if (NULL == ABC_BUF_PTR(pKeys->RQ))
            {
                // get L2
                tABC_U08Buf L2;
                ABC_CHECK_RET(ABC_AccountGetKey(szUserName, szPassword, ABC_AccountKey_L2, &L2, pError));

                // get ERQ
                int AccountNum = -1;
                ABC_CHECK_RET(ABC_AccountNumForUser(szUserName, &AccountNum, pError));
                ABC_CHECK_RET(ABC_AccountGetCarePackageObjects(AccountNum, &pJSON_ERQ, NULL, NULL, NULL, pError));

                // RQ - if ERQ available
                if (pJSON_ERQ != NULL)
                {
                    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pJSON_ERQ, L2, &(pKeys->RQ), pError));
                }
                else
                {
                    ABC_RET_ERROR(ABC_CC_NoRecoveryQuestions, "There are no recovery questions for this user");
                }
            }
            ABC_BUF_SET(*pKey, pKeys->RQ);
            break;

        default:
            ABC_RET_ERROR(ABC_CC_Error, "Unknown key type");
            break;

    };

exit:
    if (pJSON_ERQ)  json_decref(pJSON_ERQ);

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Sets the PIN for the given account
 *
 * @param szPIN PIN to use for the account
 */
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
    ABC_FREE_STR(pKeys->szPIN);
    ABC_STRDUP(pKeys->szPIN, szPIN);

    // create the PIN JSON
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(pKeys->szPIN, JSON_ACCT_PIN_FIELD, &szJSON, pError));
    tABC_U08Buf PIN = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(PIN, (unsigned char *)szJSON, strlen(szJSON) + 1);

    // EPIN = AES256(PIN, LP2)
    ABC_CHECK_RET(ABC_CryptoEncryptJSONString(PIN, pKeys->LP2, ABC_CryptoType_AES256, &szEPIN_JSON, pError));

    // write the EPIN
    ABC_CHECK_RET(ABC_AccountGetDirName(szUserName, &szAccountDir, pError));
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", szAccountDir, ACCOUNT_EPIN_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szEPIN_JSON, pError));

exit:
    ABC_FREE_STR(szJSON);
    ABC_FREE_STR(szEPIN_JSON);
    ABC_FREE_STR(szAccountDir);
    ABC_FREE_STR(szFilename);

    return cc;
}

/**
 * This function gets the categories for an account.
 * An array of allocated strings is allocated so the user is responsible for
 * free'ing all the elements as well as the array itself.
 */
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
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s/%s", szAccountDir, ACCOUNT_SYNC_DIR, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, &szJSON, pError));

    // load the strings of values
    ABC_CHECK_RET(ABC_UtilGetArrayValuesFromJSONString(szJSON, JSON_ACCT_CATEGORIES_FIELD, paszCategories, pCount, pError));

exit:
    ABC_FREE_STR(szJSON);
    ABC_FREE_STR(szAccountDir);
    ABC_FREE_STR(szFilename);

    return cc;
}

/**
 * This function adds a category to an account.
 * No attempt is made to avoid a duplicate entry.
 */
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
        ABC_ALLOC(aszCategories, sizeof(char *));
        categoryCount = 0;
    }
    ABC_STRDUP(aszCategories[categoryCount], szCategory);
    categoryCount++;

    // save out the categories
    ABC_CHECK_RET(ABC_AccountSaveCategories(szUserName, aszCategories, categoryCount, pError));

exit:
    ABC_UtilFreeStringArray(aszCategories, categoryCount);

    return cc;
}

/**
 * This function removes a category from an account.
 * If there is more than one category with this name, all categories by this name are removed.
 * If the category does not exist, no error is returned.
 */
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
                ABC_ALLOC(aszNewCategories, sizeof(char *));
                newCategoryCount = 0;
            }
            ABC_STRDUP(aszNewCategories[newCategoryCount], aszCategories[i]);
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

/**
 * Saves the categories for the given account
 */
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

    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);

    // create the categories JSON
    ABC_CHECK_RET(ABC_UtilCreateArrayJSONString(aszCategories, count, JSON_ACCT_CATEGORIES_FIELD, &szDataJSON, pError));

    // write them out
    ABC_CHECK_RET(ABC_AccountGetDirName(szUserName, &szAccountDir, pError));
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s/%s", szAccountDir, ACCOUNT_SYNC_DIR, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szDataJSON, pError));

exit:
    ABC_FREE_STR(szDataJSON);
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szAccountDir);

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
        ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
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
    ABC_FREE_STR(szAccountSyncDir);
    ABC_FREE_STR(szFilename);

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
    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_GET_QUESTIONS_PATH);

    // create base64 versions of L1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{ss}", ABC_SERVER_JSON_L1_FIELD, szL1_Base64);
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
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
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szL1_Base64);
    ABC_FREE_STR(szResults);

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
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", szRootDir, ACCOUNT_QUESTIONS_FILENAME);

    // get the JSON for the file
    szJSON = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

    // write the file
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szJSON, pError));

exit:

    if (pJSON_Root)     json_decref(pJSON_Root);
    if (pJSON_Q)        json_decref(pJSON_Q);
    ABC_FREE_STR(szRootDir);
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szJSON);

    return cc;
}

/**
 * Gets the recovery question chioces with the given info.
 *
 * @param pInfo             Pointer to recovery question chioces information
 * @param ppQuestionChoices Pointer to hold allocated pointer to recovery question chioces
 */
tABC_CC ABC_AccountGetQuestionChoices(tABC_AccountRequestInfo *pInfo,
                                      tABC_QuestionChoices    **ppQuestionChoices,
                                      tABC_Error              *pError)
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
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", szRootDir, ACCOUNT_QUESTIONS_FILENAME);

    // if the file doesn't exist
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    if (true != bExists)
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
        ABC_ALLOC(pQuestionChoices, sizeof(tABC_QuestionChoices));
        pQuestionChoices->numChoices = count;
        ABC_ALLOC(pQuestionChoices->aChoices, sizeof(tABC_QuestionChoice *) * count);

        for (int i = 0; i < count; i++)
        {
            json_t *pJSON_Elem = json_array_get(pJSON_Value, i);
            ABC_CHECK_ASSERT((pJSON_Elem && json_is_object(pJSON_Elem)), ABC_CC_JSONError, "Error parsing JSON element value for recovery questions");

            // allocate this element
            ABC_ALLOC(pQuestionChoices->aChoices[i], sizeof(tABC_QuestionChoice));

            // get the category
            json_t *pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_CATEGORY_FIELD);
            ABC_CHECK_ASSERT((pJSON_Obj && json_is_string(pJSON_Obj)), ABC_CC_JSONError, "Error parsing JSON category value for recovery questions");
            ABC_STRDUP(pQuestionChoices->aChoices[i]->szCategory, json_string_value(pJSON_Obj));

            // get the question
            pJSON_Obj = json_object_get(pJSON_Elem, ABC_SERVER_JSON_QUESTION_FIELD);
            ABC_CHECK_ASSERT((pJSON_Obj && json_is_string(pJSON_Obj)), ABC_CC_JSONError, "Error parsing JSON question value for recovery questions");
            ABC_STRDUP(pQuestionChoices->aChoices[i]->szQuestion, json_string_value(pJSON_Obj));

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
    ABC_FREE_STR(szRootDir);
    ABC_FREE_STR(szFilename);
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
                    ABC_FREE_STR(pChoice->szQuestion);
                    ABC_FREE_STR(pChoice->szCategory);
                }
            }

            ABC_CLEAR_FREE(pQuestionChoices->aChoices, sizeof(tABC_QuestionChoice *) * pQuestionChoices->numChoices);
        }

        ABC_CLEAR_FREE(pQuestionChoices, sizeof(tABC_QuestionChoices));
    }
}

/**
 * Get the recovery questions for a given account.
 *
 * The questions will be returned in a single allocated string with
 * each questions seperated by a newline.
 *
 * @param szUserName                UserName for the account
 * @param pszQuestions              Pointer into which allocated string should be stored.
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_AccountGetRecoveryQuestions(const char *szUserName,
                                        char **pszQuestions,
                                        tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(pszQuestions);
    *pszQuestions = NULL;

    // Get RQ for this user
    tABC_U08Buf RQ;
    ABC_CHECK_RET(ABC_AccountGetKey(szUserName, NULL, ABC_AccountKey_RQ, &RQ, pError));
    tABC_U08Buf FinalRQ;
    ABC_BUF_DUP(FinalRQ, RQ);
    ABC_BUF_APPEND_PTR(FinalRQ, "", 1);
    *pszQuestions = (char *)ABC_BUF_PTR(FinalRQ);

exit:

    return cc;
}

/**
 * Update the general info from the server if needed and store it in the local file.
 *
 * This function will pull down info from the server including information on
 * Obelisk Servers, AirBitz fees and miners fees if the local file doesn't exist
 * or is out of date.
 */
tABC_CC ABC_AccountServerUpdateGeneralInfo(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t  *pJSON_Root     = NULL;
    char    *szURL          = NULL;
    char    *szResults      = NULL;
    char    *szInfoFilename = NULL;
    char    *szJSON         = NULL;
    bool    bUpdateRequired = true;

    // get the info filename
    ABC_CHECK_RET(ABC_AccountGetGeneralInfoFilename(&szInfoFilename, pError));

    // check to see if we have the file
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szInfoFilename, &bExists, pError));
    if (true == bExists)
    {
        // check to see if the file is too old

        // get the current time
        time_t timeNow = time(NULL);

        // get the time the file was last changed
        time_t timeFileMod;
        ABC_CHECK_RET(ABC_FileIOFileModTime(szInfoFilename, &timeFileMod, pError));

        // if it isn't too old then don't update
        if ((timeNow - timeFileMod) < ACCOUNT_ACCEPTABLE_INFO_FILE_AGE_SECS)
        {
            bUpdateRequired = false;
        }
    }

    // if we need to update
    if (bUpdateRequired)
    {
        // create the URL
        ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
        sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_GET_INFO_PATH);

        // send the command
        ABC_CHECK_RET(ABC_URLPostString(szURL, "", &szResults, pError));
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
            // get the message
            pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_MESSAGE_FIELD);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_DebugLog("Server message: %s", json_string_value(pJSON_Value));
            ABC_RET_ERROR(ABC_CC_ServerError, json_string_value(pJSON_Value));
        }

        // get the info
        pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_RESULTS_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_object(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON info results");
        szJSON = ABC_UtilStringFromJSONObject(pJSON_Value, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

        // write the file
        ABC_CHECK_RET(ABC_FileIOWriteFileStr(szInfoFilename, szJSON, pError));
    }

exit:

    if (pJSON_Root)     json_decref(pJSON_Root);
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szResults);
    ABC_FREE_STR(szInfoFilename);
    ABC_FREE_STR(szJSON);

    return cc;
}

/**
 * Load the general info.
 *
 * This function will load the general info which includes information on
 * Obelisk Servers, AirBitz fees and miners fees.
 */
tABC_CC ABC_AccountLoadGeneralInfo(tABC_AccountGeneralInfo **ppInfo,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t  *pJSON_Root             = NULL;
    json_t  *pJSON_Value            = NULL;
    char    *szInfoFilename         = NULL;
    tABC_AccountGeneralInfo *pInfo  = NULL;

    ABC_CHECK_NULL(ppInfo);

    // get the info filename
    ABC_CHECK_RET(ABC_AccountGetGeneralInfoFilename(&szInfoFilename, pError));

    // check to see if we have the file
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szInfoFilename, &bExists, pError));
    if (false == bExists)
    {
        // pull it down from the server
        ABC_CHECK_RET(ABC_AccountServerUpdateGeneralInfo(pError));
    }

    // load the json
    ABC_CHECK_RET(ABC_FileIOReadFileObject(szInfoFilename, &pJSON_Root, true, pError));

    // allocate the struct
    ABC_ALLOC(pInfo, sizeof(tABC_AccountGeneralInfo));

    // get the miners fees array
    json_t *pJSON_MinersFeesArray = json_object_get(pJSON_Root, JSON_INFO_MINERS_FEES_FIELD);
    ABC_CHECK_ASSERT((pJSON_MinersFeesArray && json_is_array(pJSON_MinersFeesArray)), ABC_CC_JSONError, "Error parsing JSON array value");

    // get the number of elements in the array
    pInfo->countMinersFees = (unsigned int) json_array_size(pJSON_MinersFeesArray);
    if (pInfo->countMinersFees > 0)
    {
        ABC_ALLOC(pInfo->aMinersFees, pInfo->countMinersFees * sizeof(tABC_AccountMinerFee *));
    }

    // run through all the miners fees
    for (int i = 0; i < pInfo->countMinersFees; i++)
    {
        tABC_AccountMinerFee *pFee = NULL;
        ABC_ALLOC(pFee, sizeof(tABC_AccountMinerFee));

        // get the source object
        json_t *pJSON_Fee = json_array_get(pJSON_MinersFeesArray, i);
        ABC_CHECK_ASSERT((pJSON_Fee && json_is_object(pJSON_Fee)), ABC_CC_JSONError, "Error parsing JSON array element object");

        // get the satoshi amount
        pJSON_Value = json_object_get(pJSON_Fee, JSON_INFO_MINERS_FEE_SATOSHI_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pFee->amountSatoshi = (int) json_integer_value(pJSON_Value);

        // get the tranaction size
        pJSON_Value = json_object_get(pJSON_Fee, JSON_INFO_MINERS_FEE_TX_SIZE_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pFee->sizeTransaction = (int) json_integer_value(pJSON_Value);

        // assign this fee to the array
        pInfo->aMinersFees[i] = pFee;
    }

    // allocate the air bitz fees
    ABC_ALLOC(pInfo->pAirBitzFee, sizeof(tABC_AccountAirBitzFee));

    // get the air bitz fees object
    json_t *pJSON_AirBitzFees = json_object_get(pJSON_Root, JSON_INFO_AIRBITZ_FEES_FIELD);
    ABC_CHECK_ASSERT((pJSON_AirBitzFees && json_is_object(pJSON_AirBitzFees)), ABC_CC_JSONError, "Error parsing JSON object value");

    // get the air bitz fees percentage
    pJSON_Value = json_object_get(pJSON_AirBitzFees, JSON_INFO_AIRBITZ_FEE_PERCENTAGE_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_number(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON number value");
    pInfo->pAirBitzFee->percentage = json_number_value(pJSON_Value);

    // get the air bitz fees min satoshi
    pJSON_Value = json_object_get(pJSON_AirBitzFees, JSON_INFO_AIRBITZ_FEE_MIN_SATOSHI_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
    pInfo->pAirBitzFee->minSatoshi = json_integer_value(pJSON_Value);

    // get the air bitz fees max satoshi
    pJSON_Value = json_object_get(pJSON_AirBitzFees, JSON_INFO_AIRBITZ_FEE_MAX_SATOSHI_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
    pInfo->pAirBitzFee->maxSatoshi = json_integer_value(pJSON_Value);

    // get the air bitz fees address
    pJSON_Value = json_object_get(pJSON_AirBitzFees, JSON_INFO_AIRBITZ_FEE_ADDRESS_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
    ABC_STRDUP(pInfo->pAirBitzFee->szAddresss, json_string_value(pJSON_Value));


    // get the obelisk array
    json_t *pJSON_ObeliskArray = json_object_get(pJSON_Root, JSON_INFO_OBELISK_SERVERS_FIELD);
    ABC_CHECK_ASSERT((pJSON_ObeliskArray && json_is_array(pJSON_ObeliskArray)), ABC_CC_JSONError, "Error parsing JSON array value");

    // get the number of elements in the array
    pInfo->countObeliskServers = (unsigned int) json_array_size(pJSON_ObeliskArray);
    if (pInfo->countObeliskServers > 0)
    {
        ABC_ALLOC(pInfo->aszObeliskServers, pInfo->countObeliskServers * sizeof(char *));
    }

    // run through all the obelisk servers
    for (int i = 0; i < pInfo->countObeliskServers; i++)
    {
        // get the obelisk server
        pJSON_Value = json_array_get(pJSON_ObeliskArray, i);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
        ABC_STRDUP(pInfo->aszObeliskServers[i], json_string_value(pJSON_Value));
    }


    // assign the final result
    *ppInfo = pInfo;
    pInfo = NULL;

exit:

    if (pJSON_Root) json_decref(pJSON_Root);
    ABC_FREE_STR(szInfoFilename);
    ABC_AccountFreeGeneralInfo(pInfo);

    return cc;
}

/**
 * Frees the general info struct.
 */
void ABC_AccountFreeGeneralInfo(tABC_AccountGeneralInfo *pInfo)
{
    if (pInfo)
    {
        if ((pInfo->aMinersFees != NULL) && (pInfo->countMinersFees > 0))
        {
            for (int i = 0; i < pInfo->countMinersFees; i++)
            {
                ABC_CLEAR_FREE(pInfo->aMinersFees[i], sizeof(tABC_AccountMinerFee));
            }
            ABC_CLEAR_FREE(pInfo->aMinersFees, sizeof(tABC_AccountMinerFee *) * pInfo->countMinersFees);
        }

        if (pInfo->pAirBitzFee)
        {
            ABC_FREE_STR(pInfo->pAirBitzFee->szAddresss);
            ABC_CLEAR_FREE(pInfo->pAirBitzFee, sizeof(tABC_AccountMinerFee));
        }

        if ((pInfo->aszObeliskServers != NULL) && (pInfo->countObeliskServers > 0))
        {
            for (int i = 0; i < pInfo->countObeliskServers; i++)
            {
                ABC_FREE_STR(pInfo->aszObeliskServers[i]);
            }
            ABC_CLEAR_FREE(pInfo->aszObeliskServers, sizeof(char *) * pInfo->countObeliskServers);
        }

        ABC_CLEAR_FREE(pInfo, sizeof(tABC_AccountGeneralInfo));
    }
}

/*
 * Gets the general info filename
 *
 * @param pszFilename Location to store allocated filename string (caller must free)
 */
static
tABC_CC ABC_AccountGetGeneralInfoFilename(char **pszFilename,
                                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szRootDir = NULL;

    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;

    ABC_CHECK_RET(ABC_AccountGetRootDir(&szRootDir, pError));
    ABC_ALLOC(*pszFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszFilename, "%s/%s", szRootDir, ACCOUNT_INFO_FILENAME);

exit:
    ABC_FREE_STR(szRootDir);

    return cc;
}

/*
 * Gets the account settings filename for a given username
 *
 * @param pszFilename Location to store allocated filename string (caller must free)
 */
static
tABC_CC ABC_AccountGetSettingsFilename(const char *szUserName,
                                       char **pszFilename,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szSyncDirName = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;

    ABC_CHECK_RET(ABC_AccountGetSyncDirName(szUserName, &szSyncDirName, pError));

    ABC_ALLOC(*pszFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszFilename, "%s/%s", szSyncDirName, ACCOUNT_SETTINGS_FILENAME);

exit:
    ABC_FREE_STR(szSyncDirName);

    return cc;
}

/**
 * Creates default account settings
 *
 * @param ppSettings    Location to store ptr to allocated settings (caller must free)
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_AccountCreateDefaultSettings(tABC_AccountSettings **ppSettings,
                                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_AccountSettings *pSettings = NULL;

    ABC_CHECK_NULL(ppSettings);

    ABC_ALLOC(pSettings, sizeof(tABC_AccountSettings));

    pSettings->szFirstName = NULL;
    pSettings->szLastName = NULL;
    pSettings->szNickname = NULL;
    pSettings->bNameOnPayments = false;
    pSettings->minutesAutoLogout = 60;
    ABC_STRDUP(pSettings->szLanguage, "en");
    pSettings->currencyNum = CURRENCY_NUM_USD;

    pSettings->exchangeRateSources.numSources = 0;
    pSettings->exchangeRateSources.aSources = NULL;

    ABC_STRDUP(pSettings->bitcoinDenomination.szLabel, "mBTC");
    pSettings->bitcoinDenomination.satoshi = 100000;

    // assign final settings
    *ppSettings = pSettings;
    pSettings = NULL;

exit:
    ABC_AccountFreeSettings(pSettings);

    return cc;
}

/**
 * Loads the settings for a specific account using the given key
 * If no settings file exists for the given user, defaults are created
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param Key           Key used to decrypt the settings
 * @param ppSettings    Location to store ptr to allocated settings (caller must free)
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_AccountLoadSettingsEnc(const char *szUserName,
                                   tABC_U08Buf Key,
                                   tABC_AccountSettings **ppSettings,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_AccountSettings *pSettings = NULL;
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;
    json_t *pJSON_Value = NULL;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(ppSettings);

    // get the settings filename
    ABC_CHECK_RET(ABC_AccountGetSettingsFilename(szUserName, &szFilename, pError));

    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    if (true == bExists)
    {
        // load and decrypted the file into a json object
        ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, Key, &pJSON_Root, pError));
        //ABC_DebugLog("Loaded settings JSON:\n%s\n", json_dumps(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER));

        // allocate the new settings object
        ABC_ALLOC(pSettings, sizeof(tABC_AccountSettings));

        // get the first name
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_FIRST_NAME_FIELD);
        if (pJSON_Value)
        {
            ABC_CHECK_ASSERT(json_is_string(pJSON_Value), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_STRDUP(pSettings->szFirstName, json_string_value(pJSON_Value));
        }

        // get the last name
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_LAST_NAME_FIELD);
        if (pJSON_Value)
        {
            ABC_CHECK_ASSERT(json_is_string(pJSON_Value), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_STRDUP(pSettings->szLastName, json_string_value(pJSON_Value));
        }

        // get the nickname
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_NICKNAME_FIELD);
        if (pJSON_Value)
        {
            ABC_CHECK_ASSERT(json_is_string(pJSON_Value), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_STRDUP(pSettings->szNickname, json_string_value(pJSON_Value));
        }

        // get name on payments option
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_NAME_ON_PAYMENTS_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_boolean(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON boolean value");
        pSettings->bNameOnPayments = json_is_true(pJSON_Value) ? true : false;

        // get minutes auto logout
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_MINUTES_AUTO_LOGOUT_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pSettings->minutesAutoLogout = (int) json_integer_value(pJSON_Value);

        // get language
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_LANGUAGE_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
        ABC_STRDUP(pSettings->szLanguage, json_string_value(pJSON_Value));

        // get currency num
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_NUM_CURRENCY_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pSettings->currencyNum = (int) json_integer_value(pJSON_Value);

        // get advanced features
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_ADVANCED_FEATURES_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_boolean(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON boolean value");
        pSettings->bAdvancedFeatures = json_is_true(pJSON_Value) ? true : false;

        // get the denomination object
        json_t *pJSON_Denom = json_object_get(pJSON_Root, JSON_ACCT_BITCOIN_DENOMINATION_FIELD);
        ABC_CHECK_ASSERT((pJSON_Denom && json_is_object(pJSON_Denom)), ABC_CC_JSONError, "Error parsing JSON object value");

        // get denomination satoshi display size (e.g., 100,000 would be milli-bit coin)
        pJSON_Value = json_object_get(pJSON_Denom, JSON_ACCT_SATOSHI_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pSettings->bitcoinDenomination.satoshi = json_integer_value(pJSON_Value);

        // get denomination label
        pJSON_Value = json_object_get(pJSON_Denom, JSON_ACCT_LABEL_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
        ABC_STRDUP(pSettings->bitcoinDenomination.szLabel, json_string_value(pJSON_Value));

        // get the exchange rates array
        json_t *pJSON_Sources = json_object_get(pJSON_Root, JSON_ACCT_EX_RATE_SOURCES_FIELD);
        ABC_CHECK_ASSERT((pJSON_Sources && json_is_array(pJSON_Sources)), ABC_CC_JSONError, "Error parsing JSON array value");

        // get the number of elements in the array
        pSettings->exchangeRateSources.numSources = (int) json_array_size(pJSON_Sources);
        if (pSettings->exchangeRateSources.numSources > 0)
        {
            ABC_ALLOC(pSettings->exchangeRateSources.aSources, pSettings->exchangeRateSources.numSources * sizeof(tABC_ExchangeRateSource *));
        }

        // run through all the sources
        for (int i = 0; i < pSettings->exchangeRateSources.numSources; i++)
        {
            tABC_ExchangeRateSource *pSource = NULL;
            ABC_ALLOC(pSource, sizeof(tABC_ExchangeRateSource));

            // get the source object
            json_t *pJSON_Source = json_array_get(pJSON_Sources, i);
            ABC_CHECK_ASSERT((pJSON_Source && json_is_object(pJSON_Source)), ABC_CC_JSONError, "Error parsing JSON array element object");

            // get the currency num
            pJSON_Value = json_object_get(pJSON_Source, JSON_ACCT_NUM_CURRENCY_FIELD);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
            pSource->currencyNum = (int) json_integer_value(pJSON_Value);

            // get the exchange rate source
            pJSON_Value = json_object_get(pJSON_Source, JSON_ACCT_EX_RATE_SOURCE_FIELD);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_STRDUP(pSource->szSource, json_string_value(pJSON_Value));

            // assign this source to the array
            pSettings->exchangeRateSources.aSources[i] = pSource;
        }
    }
    else
    {
        // create the defaults
        ABC_CHECK_RET(ABC_AccountCreateDefaultSettings(&pSettings, pError));
    }

    // assign final settings
    *ppSettings = pSettings;
    pSettings = NULL;

exit:
    ABC_AccountFreeSettings(pSettings);
    ABC_FREE_STR(szFilename);
    if (pJSON_Root) json_decref(pJSON_Root);

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Loads the settings for a specific account
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param szPassword    Password for the account associated with the settings
 * @param ppSettings    Location to store ptr to allocated settings (caller must free)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_AccountLoadSettings(const char *szUserName,
                                const char *szPassword,
                                tABC_AccountSettings **ppSettings,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf LP2 = ABC_BUF_NULL;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(ppSettings);

    // get LP2
    ABC_CHECK_RET(ABC_AccountGetKey(szUserName,szPassword, ABC_AccountKey_LP2, &LP2, pError));

    // load them with the given key
    ABC_CHECK_RET(ABC_AccountLoadSettingsEnc(szUserName, LP2, ppSettings, pError));

exit:

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Saves the settings for a specific account using the given key
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param Key           Key used to encrypt the settings
 * @param pSettings     Pointer to settings
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_AccountSaveSettingsEnc(const char *szUserName,
                                   tABC_U08Buf Key,
                                   tABC_AccountSettings *pSettings,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    json_t *pJSON_Root = NULL;
    json_t *pJSON_Denom = NULL;
    json_t *pJSON_SourcesArray = NULL;
    json_t *pJSON_Source = NULL;
    char *szFilename = NULL;
    int retVal = 0;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pSettings);

    // create the json for the settings
    pJSON_Root = json_object();
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_Error, "Could not create settings JSON object");

    // set the first name
    if (pSettings->szFirstName)
    {
        retVal = json_object_set_new(pJSON_Root, JSON_ACCT_FIRST_NAME_FIELD, json_string(pSettings->szFirstName));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");
    }

    // set the last name
    if (pSettings->szLastName)
    {
        retVal = json_object_set_new(pJSON_Root, JSON_ACCT_LAST_NAME_FIELD, json_string(pSettings->szLastName));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");
    }

    // set the nickname
    if (pSettings->szNickname)
    {
        retVal = json_object_set_new(pJSON_Root, JSON_ACCT_NICKNAME_FIELD, json_string(pSettings->szNickname));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");
    }

    // set name on payments option
    retVal = json_object_set_new(pJSON_Root, JSON_ACCT_NAME_ON_PAYMENTS_FIELD, json_boolean(pSettings->bNameOnPayments));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // set minutes auto logout
    retVal = json_object_set_new(pJSON_Root, JSON_ACCT_MINUTES_AUTO_LOGOUT_FIELD, json_integer(pSettings->minutesAutoLogout));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // set language
    retVal = json_object_set_new(pJSON_Root, JSON_ACCT_LANGUAGE_FIELD, json_string(pSettings->szLanguage));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // set currency num
    retVal = json_object_set_new(pJSON_Root, JSON_ACCT_NUM_CURRENCY_FIELD, json_integer(pSettings->currencyNum));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // set advanced features
    retVal = json_object_set_new(pJSON_Root, JSON_ACCT_ADVANCED_FEATURES_FIELD, json_boolean(pSettings->bAdvancedFeatures));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // create the denomination section
    pJSON_Denom = json_object();
    ABC_CHECK_ASSERT(pJSON_Denom != NULL, ABC_CC_Error, "Could not create settings JSON object");

    // set denomination satoshi display size (e.g., 100,000 would be milli-bit coin)
    retVal = json_object_set_new(pJSON_Denom, JSON_ACCT_SATOSHI_FIELD, json_integer(pSettings->bitcoinDenomination.satoshi));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // set denomination label
    retVal = json_object_set_new(pJSON_Denom, JSON_ACCT_LABEL_FIELD, json_string(pSettings->bitcoinDenomination.szLabel));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the denomination object
    retVal = json_object_set(pJSON_Root, JSON_ACCT_BITCOIN_DENOMINATION_FIELD, pJSON_Denom);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // create the exchange sources array
    pJSON_SourcesArray = json_array();
    ABC_CHECK_ASSERT(pJSON_SourcesArray != NULL, ABC_CC_Error, "Could not create settings JSON object");

    // add the exchange sources
    for (int i = 0; i < pSettings->exchangeRateSources.numSources; i++)
    {
        tABC_ExchangeRateSource *pSource = pSettings->exchangeRateSources.aSources[i];

        // create the source object
        pJSON_Source = json_object();
        ABC_CHECK_ASSERT(pJSON_Source != NULL, ABC_CC_Error, "Could not create settings JSON object");

        // set the currency num
        retVal = json_object_set_new(pJSON_Source, JSON_ACCT_NUM_CURRENCY_FIELD, json_integer(pSource->currencyNum));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

        // set the exchange rate source
        retVal = json_object_set_new(pJSON_Source, JSON_ACCT_EX_RATE_SOURCE_FIELD, json_string(pSource->szSource));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

        // append this object to our array
        retVal = json_array_append(pJSON_SourcesArray, pJSON_Source);
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

        // free the source object
        if (pJSON_Source) json_decref(pJSON_Source);
        pJSON_Source = NULL;
    }

    // add the exchange sources array object
    retVal = json_object_set(pJSON_Root, JSON_ACCT_EX_RATE_SOURCES_FIELD, pJSON_SourcesArray);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // get the settings filename
    ABC_CHECK_RET(ABC_AccountGetSettingsFilename(szUserName, &szFilename, pError));

    // encrypt and save json
    //ABC_DebugLog("Saving settings JSON:\n%s\n", json_dumps(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, Key, ABC_CryptoType_AES256, szFilename, pError));


exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    if (pJSON_Denom) json_decref(pJSON_Denom);
    if (pJSON_SourcesArray) json_decref(pJSON_SourcesArray);
    if (pJSON_Source) json_decref(pJSON_Source);
    ABC_FREE_STR(szFilename);

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Saves the settings for a specific account
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param szPassword    Password for the account associated with the settings
 * @param pSettings     Pointer to settings
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_AccountSaveSettings(const char *szUserName,
                                const char *szPassword,
                                tABC_AccountSettings *pSettings,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf LP2 = ABC_BUF_NULL;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(pSettings);

    // get LP2
    ABC_CHECK_RET(ABC_AccountGetKey(szUserName, szPassword, ABC_AccountKey_LP2, &LP2, pError));

    // save them with the given key
    ABC_CHECK_RET(ABC_AccountSaveSettingsEnc(szUserName, LP2, pSettings, pError));

exit:

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Free's the given account settings
 */
void ABC_AccountFreeSettings(tABC_AccountSettings *pSettings)
{
    if (pSettings)
    {
        ABC_FREE_STR(pSettings->szFirstName);
        ABC_FREE_STR(pSettings->szLastName);
        ABC_FREE_STR(pSettings->szNickname);
        ABC_FREE_STR(pSettings->szLanguage);
        ABC_FREE_STR(pSettings->bitcoinDenomination.szLabel);
        if (pSettings->exchangeRateSources.aSources)
        {
            for (int i = 0; i < pSettings->exchangeRateSources.numSources; i++)
            {
                ABC_FREE_STR(pSettings->exchangeRateSources.aSources[i]->szSource);
                ABC_CLEAR_FREE(pSettings->exchangeRateSources.aSources[i], sizeof(tABC_ExchangeRateSource));
            }
            ABC_CLEAR_FREE(pSettings->exchangeRateSources.aSources, sizeof(tABC_ExchangeRateSource *) * pSettings->exchangeRateSources.numSources);
        }

        ABC_CLEAR_FREE(pSettings, sizeof(tABC_AccountSettings));
    }
}

/**
 * Locks the mutex
 *
 * ABC_Wallet uses the same mutex as ABC_Account so that there will be no situation in
 * which one thread is in ABC_Wallet locked on a mutex and calling a thread safe ABC_Account call
 * that is locked from another thread calling a thread safe ABC_Wallet call.
 * In other words, since they call each other, they need to share a recursive mutex.
 */
static
tABC_CC ABC_AccountMutexLock(tABC_Error *pError)
{
    return ABC_MutexLock(pError);
}

/**
 * Unlocks the mutex
 *
 */
static
tABC_CC ABC_AccountMutexUnlock(tABC_Error *pError)
{
    return ABC_MutexUnlock(pError);
}
