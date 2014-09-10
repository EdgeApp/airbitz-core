/**
 * @file
 * AirBitz SQL server API.
 */

#include "ABC_LoginServer.h"
#include "ABC_Login.h"
#include "ABC_ServerDefs.h"
#include "ABC_URL.h"
#include "ABC_Util.h"

// For debug upload:
#include "ABC_Bridge.h"
#include "ABC_Crypto.h"
#include "ABC_FileIO.h"

// Server strings:
#define JSON_ACCT_CARE_PACKAGE                  "care_package"
#define JSON_ACCT_LOGIN_PACKAGE                 "login_package"

static tABC_CC ABC_LoginServerGetString(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_U08Buf LRA1, char *szURL, char *szField, char **szResponse, tABC_Error *pError);

/**
 * Creates an account on the server.
 *
 * This function sends information to the server to create an account.
 * If the account was created, ABC_CC_Ok is returned.
 * If the account already exists, ABC_CC_AccountAlreadyExists is returned.
 *
 * @param L1   Login hash for the account
 * @param LP1  Password hash for the account
 */
tABC_CC ABC_LoginServerCreate(tABC_U08Buf L1,
                              tABC_U08Buf LP1,
                              const char *szCarePackage_JSON,
                              const char *szLoginPackage_JSON,
                              char *szRepoAcctKey,
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
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_ACCOUNT_CREATE_PATH);

    // create base64 versions of L1 and LP1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szLP1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{ssssssssss}",
                        ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                        ABC_SERVER_JSON_LP1_FIELD, szLP1_Base64,
                        ABC_SERVER_JSON_CARE_PACKAGE_FIELD, szCarePackage_JSON,
                        ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, szLoginPackage_JSON,
                        ABC_SERVER_JSON_REPO_FIELD, szRepoAcctKey);
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    ABC_DebugLog("Server URL: %s, Data: %.50s", szURL, szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %.50s", szResults);

    // decode the result
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
 * Activate an account on the server.
 *
 * @param L1   Login hash for the account
 * @param LP1  Password hash for the account
 */
tABC_CC ABC_LoginServerActivate(tABC_U08Buf L1,
                                tABC_U08Buf LP1,
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
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_ACCOUNT_ACTIVATE);

    // create base64 versions of L1 and LP1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szLP1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{ssss}",
                        ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                        ABC_SERVER_JSON_LP1_FIELD, szLP1_Base64);
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %.50s", szURL, szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %.50s", szResults);

    // decode the result
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
 * Set recovery questions and answers on the server.
 *
 * This function sends LRA1 and Care Package to the server as part
 * of setting up the recovery data for an account
 *
 * @param L1             Login hash for the account
 * @param LP1            Password hash for the account
 * @param LRA1           Scrypt'ed login and recovery answers
 * @param szCarePackage  Care Package for account
 * @param szLoginPackage Login Package for account
 */
tABC_CC ABC_LoginServerSetRecovery(tABC_U08Buf L1, tABC_U08Buf LP1,
                                   tABC_U08Buf LRA1,
                                   const char *szCarePackage,
                                   const char *szLoginPackage,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szURL         = NULL;
    char *szResults     = NULL;
    char *szPost        = NULL;
    char *szL1_Base64   = NULL;
    char *szLP1_Base64  = NULL;
    char *szLRA1_Base64 = NULL;
    json_t *pJSON_Root  = NULL;

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL(szCarePackage);
    ABC_CHECK_NULL(szLoginPackage);

    // create the URL
    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_UPDATE_CARE_PACKAGE_PATH);

    // create base64 versions of L1, LP1 and LRA1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szLP1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LRA1, &szLRA1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{ssssssssss}",
                        ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                        ABC_SERVER_JSON_LP1_FIELD, szLP1_Base64,
                        ABC_SERVER_JSON_LRA1_FIELD, szLRA1_Base64,
                        ABC_SERVER_JSON_CARE_PACKAGE_FIELD, szCarePackage,
                        ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, szLoginPackage);

    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    ABC_DebugLog("Server URL: %s, Data: %.50s", szURL, szPost);

    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %.50s", szResults);

    ABC_CHECK_RET(ABC_URLCheckResults(szResults, NULL, pError));
exit:
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szResults);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szL1_Base64);
    ABC_FREE_STR(szLP1_Base64);
    ABC_FREE_STR(szLRA1_Base64);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

/**
 * Changes the password for an account on the server.
 *
 * This function sends information to the server to change the password for an account.
 * Either the old LP1 or LRA1 can be used for authentication.
 *
 * @param L1     Login hash for the account
 * @param oldLP1 Old password hash for the account (if this is empty, LRA1 is used instead)
 * @param LRA1   LRA1 for the account (used if oldP1 is empty)
 */
tABC_CC ABC_LoginServerChangePassword(tABC_U08Buf L1,
                                      tABC_U08Buf oldLP1,
                                      tABC_U08Buf LRA1,
                                      tABC_U08Buf newP1,
                                      char *szLoginPackage,
                                      tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szURL     = NULL;
    char *szResults = NULL;
    char *szPost    = NULL;
    char *szL1_Base64 = NULL;
    char *szOldP1_Base64 = NULL;
    char *szNewLP1_Base64 = NULL;
    char *szAuth_Base64 = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL_BUF(newP1);

    // create the URL
    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_CHANGE_PASSWORD_PATH);

    // create base64 versions of L1 and newP1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(newP1, &szNewLP1_Base64, pError));

    // create the post data
    if (ABC_BUF_PTR(oldLP1) != NULL)
    {
        ABC_CHECK_RET(ABC_CryptoBase64Encode(oldLP1, &szAuth_Base64, pError));
        pJSON_Root = json_pack("{ssssssss}",
                               ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                               ABC_SERVER_JSON_LP1_FIELD, szAuth_Base64,
                               ABC_SERVER_JSON_NEW_LP1_FIELD, szNewLP1_Base64,
                               ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, szLoginPackage);
    }
    else
    {
        ABC_CHECK_ASSERT(NULL != ABC_BUF_PTR(LRA1), ABC_CC_Error, "LRA1 missing for server password change auth");
        ABC_CHECK_RET(ABC_CryptoBase64Encode(LRA1, &szAuth_Base64, pError));
        pJSON_Root = json_pack("{ssssssss}",
                               ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                               ABC_SERVER_JSON_LRA1_FIELD, szAuth_Base64,
                               ABC_SERVER_JSON_NEW_LP1_FIELD, szNewLP1_Base64,
                               ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, szLoginPackage);
    }
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    ABC_DebugLog("Server URL: %s, Data: %.50s", szURL, szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %.50s", szResults);

    ABC_CHECK_RET(ABC_URLCheckResults(szResults, NULL, pError));
exit:
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szResults);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szL1_Base64);
    ABC_FREE_STR(szOldP1_Base64);
    ABC_FREE_STR(szNewLP1_Base64);
    ABC_FREE_STR(szAuth_Base64);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

tABC_CC ABC_LoginServerGetCarePackage(tABC_U08Buf L1,
                                      char **szCarePackage,
                                      tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szURL = NULL;
    tABC_U08Buf LP1_NULL = ABC_BUF_NULL;
    tABC_U08Buf LRA1 = ABC_BUF_NULL;

    ABC_CHECK_NULL_BUF(L1);

    // create the URL
    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_GET_CARE_PACKAGE_PATH);

    ABC_CHECK_RET(ABC_LoginServerGetString(L1, LP1_NULL, LRA1, szURL, JSON_ACCT_CARE_PACKAGE, szCarePackage, pError));
exit:

    ABC_FREE_STR(szURL);

    return cc;
}

tABC_CC ABC_LoginServerGetLoginPackage(tABC_U08Buf L1,
                                       tABC_U08Buf LP1,
                                       tABC_U08Buf LRA1,
                                       char **szLoginPackage,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szURL = NULL;

    ABC_CHECK_NULL_BUF(L1);

    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_LOGIN_PACK_GET_PATH);

    ABC_CHECK_RET(ABC_LoginServerGetString(L1, LP1, LRA1, szURL, JSON_ACCT_LOGIN_PACKAGE, szLoginPackage, pError));
exit:

    ABC_FREE_STR(szURL);

    return cc;
}

/**
 * Helper function for getting CarePackage or LoginPackage.
 */
static
tABC_CC ABC_LoginServerGetString(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_U08Buf LRA1,
                                 char *szURL, char *szField, char **szResponse, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t  *pJSON_Value    = NULL;
    json_t  *pJSON_Root     = NULL;
    char    *szPost         = NULL;
    char    *szL1_Base64    = NULL;
    char    *szAuth_Base64  = NULL;
    char    *szResults      = NULL;

    ABC_CHECK_NULL_BUF(L1);

    // create base64 versions of L1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));

    // create the post data with or without LP1
    if (ABC_BUF_PTR(LP1) == NULL && ABC_BUF_PTR(LRA1) == NULL)
    {
        pJSON_Root = json_pack("{ss}", ABC_SERVER_JSON_L1_FIELD, szL1_Base64);
    }
    else
    {
        if (ABC_BUF_PTR(LP1) == NULL)
        {
            ABC_CHECK_RET(ABC_CryptoBase64Encode(LRA1, &szAuth_Base64, pError));
            pJSON_Root = json_pack("{ssss}", ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                                             ABC_SERVER_JSON_LRA1_FIELD, szAuth_Base64);
        }
        else
        {
            ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szAuth_Base64, pError));
            pJSON_Root = json_pack("{ssss}", ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                                             ABC_SERVER_JSON_LP1_FIELD, szAuth_Base64);
        }
    }
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %s", szURL, szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %.50s", szResults);

    // Check the result, and store json if successful
    ABC_CHECK_RET(ABC_URLCheckResults(szResults, &pJSON_Root, pError));

    // get the care package
    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_RESULTS_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_object(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON care package results");

    pJSON_Value = json_object_get(pJSON_Value, szField);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error care package JSON results");
    ABC_STRDUP(*szResponse, json_string_value(pJSON_Value));
exit:
    if (pJSON_Root)     json_decref(pJSON_Root);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szL1_Base64);
    ABC_FREE_STR(szAuth_Base64);
    ABC_FREE_STR(szResults);

    return cc;
}

/**
 * Upload files to auth server for debugging
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param szPassword    Password for the account associated with the settings
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_LoginServerUploadLogs(const char *szUserName,
                                  const char *szPassword,
                                  tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szL1_Base64     = NULL;
    char *szLP1_Base64    = NULL;
    char *szPost          = NULL;
    char *szResults       = NULL;
    char *szURL           = NULL;
    char *szLogFilename   = NULL;
    char *szLogData       = NULL;
    char *szLogData_Hex   = NULL;
    char *szWatchFilename = NULL;
    void *szWatchData     = NULL;
    char *szWatchData_Hex = NULL;
    json_t *pJSON_Root    = NULL;
    tABC_U08Buf L1        = ABC_BUF_NULL; // Do not free
    tABC_U08Buf LP1       = ABC_BUF_NULL; // Do not free
    tABC_U08Buf LogData   = ABC_BUF_NULL;
    tABC_U08Buf WatchData = ABC_BUF_NULL;
    size_t watcherSize    = 0;
    unsigned int nCount   = 0;
    tABC_WalletInfo **paWalletInfo = NULL;

    // create the URL
    ABC_ALLOC(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_DEBUG_PATH);

    ABC_CHECK_RET(ABC_LoginGetServerKeys(szUserName, szPassword, &L1, &LP1, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szLP1_Base64, pError));

    ABC_CHECK_RET(ABC_DebugLogFilename(&szLogFilename, pError);)
    ABC_CHECK_RET(ABC_FileIOReadFileStr(szLogFilename, &szLogData, pError));
    ABC_BUF_SET_PTR(LogData, (unsigned char *)szLogData, strlen(szLogData) + 1);
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LogData, &szLogData_Hex, pError));

    ABC_CHECK_RET(ABC_GetWallets(szUserName, szPassword, &paWalletInfo, &nCount, pError));
    json_t *pJSON_array = json_array();
    for (int i = 0; i < nCount; ++i)
    {
        ABC_CHECK_RET(ABC_BridgeWatchPath(szUserName, szPassword,
                                          paWalletInfo[i]->szUUID,
                                          &szWatchFilename, pError));
        ABC_CHECK_RET(ABC_FileIOReadFile(szWatchFilename, &szWatchData, &watcherSize, pError));
        ABC_BUF_SET_PTR(WatchData, szWatchData, watcherSize);
        ABC_CHECK_RET(ABC_CryptoBase64Encode(WatchData, &szWatchData_Hex, pError));

        json_t *element = json_pack("s", szWatchData_Hex);
        json_array_append_new(pJSON_array, element);

        ABC_FREE_STR(szWatchFilename);
        ABC_FREE(szWatchData);
        ABC_BUF_CLEAR(WatchData);
    }

    pJSON_Root = json_pack("{ss, ss, ss}",
                            ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                            ABC_SERVER_JSON_LP1_FIELD, szLP1_Base64,
                            "log", szLogData_Hex);
    json_object_set(pJSON_Root, "watchers", pJSON_array);

    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    ABC_CHECK_RET(ABC_URLPostString(
        szURL, szPost, &szResults, pError));
    ABC_DebugLog("%s\n", szResults);
exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    if (pJSON_array) json_decref(pJSON_array);
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szResults);
    ABC_FREE_STR(szLogFilename);
    ABC_FREE_STR(szLogData);
    ABC_FREE_STR(szLogData_Hex);
    ABC_BUF_CLEAR(LogData);

    ABC_FREE_STR(szWatchFilename);
    ABC_FREE(szWatchData);
    ABC_FREE_STR(szWatchData_Hex);
    ABC_BUF_CLEAR(WatchData);

    ABC_FreeWalletInfoArray(paWalletInfo, nCount);


    return cc;
}
