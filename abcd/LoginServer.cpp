/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginServer.hpp"
#include "ServerDefs.hpp"
#include "util/Json.hpp"
#include "util/URL.hpp"
#include "util/Util.hpp"

// For debug upload:
#include "Bridge.hpp"
#include "util/Crypto.hpp"
#include "util/FileIO.hpp"

namespace abcd {

// Server strings:
#define JSON_ACCT_CARE_PACKAGE                  "care_package"
#define JSON_ACCT_LOGIN_PACKAGE                 "login_package"
#define JSON_ACCT_PIN_PACKAGE                   "pin_package"

#define DATETIME_LENGTH 20

static tABC_CC ABC_LoginServerGetString(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_U08Buf LRA1, const char *szURL, const char *szField, char **szResponse, tABC_Error *pError);

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
                              tABC_CarePackage *pCarePackage,
                              tABC_LoginPackage *pLoginPackage,
                              char *szRepoAcctKey,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szURL     = NULL;
    char *szResults = NULL;
    char *szPost    = NULL;
    char *szL1_Base64 = NULL;
    char *szLP1_Base64 = NULL;
    char *szCarePackage     = NULL;
    char *szLoginPackage    = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL_BUF(LP1);

    // create the URL
    ABC_STR_NEW(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_ACCOUNT_CREATE_PATH);

    // create base64 versions of L1 and LP1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szLP1_Base64, pError));

    ABC_CHECK_RET(ABC_CarePackageEncode(pCarePackage, &szCarePackage, pError));
    ABC_CHECK_RET(ABC_LoginPackageEncode(pLoginPackage, &szLoginPackage, pError));

    // create the post data
    pJSON_Root = json_pack("{ssssssssss}",
                        ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                        ABC_SERVER_JSON_LP1_FIELD, szLP1_Base64,
                        ABC_SERVER_JSON_CARE_PACKAGE_FIELD, szCarePackage,
                        ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, szLoginPackage,
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
    ABC_FREE_STR(szCarePackage);
    ABC_FREE_STR(szLoginPackage);
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
    ABC_STR_NEW(szURL, ABC_URL_MAX_PATH_LENGTH);
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
                                      tABC_U08Buf newLP1,
                                      tABC_U08Buf newLRA1,
                                      tABC_CarePackage *pCarePackage,
                                      tABC_LoginPackage *pLoginPackage,
                                      tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szURL     = NULL;
    char *szResults = NULL;
    char *szPost    = NULL;
    char *szBase64_L1       = NULL;
    char *szBase64_OldLP1   = NULL;
    char *szBase64_NewLP1   = NULL;
    char *szBase64_NewLRA1  = NULL;
    char *szCarePackage     = NULL;
    char *szLoginPackage    = NULL;
    json_t *pJSON_OldLRA1   = NULL;
    json_t *pJSON_NewLRA1   = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL_BUF(oldLP1);
    ABC_CHECK_NULL_BUF(newLP1);

    // create the URL
    ABC_STR_NEW(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_CHANGE_PASSWORD_PATH);

    // create base64 versions of L1, oldLP1, and newLP1:
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1,     &szBase64_L1,     pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(oldLP1, &szBase64_OldLP1, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(newLP1, &szBase64_NewLP1, pError));

    ABC_CHECK_RET(ABC_CarePackageEncode(pCarePackage, &szCarePackage, pError));
    ABC_CHECK_RET(ABC_LoginPackageEncode(pLoginPackage, &szLoginPackage, pError));

    // Encode those:
    pJSON_Root = json_pack("{ss, ss, ss, ss, ss}",
                           ABC_SERVER_JSON_L1_FIELD,      szBase64_L1,
                           ABC_SERVER_JSON_LP1_FIELD,     szBase64_OldLP1,
                           ABC_SERVER_JSON_NEW_LP1_FIELD, szBase64_NewLP1,
                           ABC_SERVER_JSON_CARE_PACKAGE_FIELD,  szCarePackage,
                           ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, szLoginPackage);
    ABC_CHECK_NULL(pJSON_Root);

    // set up the recovery, if any:
    if (ABC_BUF_PTR(newLRA1))
    {
        ABC_CHECK_RET(ABC_CryptoBase64Encode(newLRA1, &szBase64_NewLRA1, pError));
        pJSON_NewLRA1 = json_string(szBase64_NewLRA1);
        json_object_set(pJSON_Root, ABC_SERVER_JSON_NEW_LRA1_FIELD, pJSON_NewLRA1);
    }

    // create the post data
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
    ABC_FREE_STR(szBase64_L1);
    ABC_FREE_STR(szBase64_OldLP1);
    ABC_FREE_STR(szBase64_NewLP1);
    ABC_FREE_STR(szBase64_NewLRA1);
    ABC_FREE_STR(szCarePackage);
    ABC_FREE_STR(szLoginPackage);
    if (pJSON_OldLRA1)  json_decref(pJSON_OldLRA1);
    if (pJSON_NewLRA1)  json_decref(pJSON_NewLRA1);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

tABC_CC ABC_LoginServerGetCarePackage(tABC_U08Buf L1,
                                      tABC_CarePackage **ppCarePackage,
                                      tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szURL = NULL;
    tABC_U08Buf LP1_NULL = ABC_BUF_NULL;
    tABC_U08Buf LRA1 = ABC_BUF_NULL;
    char *szCarePackage = NULL;

    ABC_CHECK_NULL_BUF(L1);

    // create the URL
    ABC_STR_NEW(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_GET_CARE_PACKAGE_PATH);

    ABC_CHECK_RET(ABC_LoginServerGetString(L1, LP1_NULL, LRA1, szURL, JSON_ACCT_CARE_PACKAGE, &szCarePackage, pError));
    ABC_CHECK_RET(ABC_CarePackageDecode(ppCarePackage, szCarePackage, pError));

exit:
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szCarePackage);

    return cc;
}

tABC_CC ABC_LoginServerGetLoginPackage(tABC_U08Buf L1,
                                       tABC_U08Buf LP1,
                                       tABC_U08Buf LRA1,
                                       tABC_LoginPackage **ppLoginPackage,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szURL = NULL;
    char *szLoginPackage = NULL;

    ABC_CHECK_NULL_BUF(L1);

    ABC_STR_NEW(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_LOGIN_PACK_GET_PATH);

    ABC_CHECK_RET(ABC_LoginServerGetString(L1, LP1, LRA1, szURL, JSON_ACCT_LOGIN_PACKAGE, &szLoginPackage, pError));
    ABC_CHECK_RET(ABC_LoginPackageDecode(ppLoginPackage, szLoginPackage, pError));

exit:
    ABC_FREE_STR(szURL);
    ABC_FREE_STR(szLoginPackage);

    return cc;
}

/**
 * Helper function for getting CarePackage or LoginPackage.
 */
static
tABC_CC ABC_LoginServerGetString(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_U08Buf LRA1,
                                 const char *szURL, const char *szField, char **szResponse, tABC_Error *pError)
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

    // Check the results, and store json if successful
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

tABC_CC ABC_LoginServerGetPinPackage(tABC_U08Buf DID,
                                     tABC_U08Buf LPIN1,
                                     char **szPinPackage,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t  *pJSON_Value    = NULL;
    json_t  *pJSON_Root     = NULL;
    char    *szURL          = NULL;

    char    *szPost         = NULL;
    char    *szDid_Base64   = NULL;
    char    *szLPIN1_Base64 = NULL;
    char    *szResults      = NULL;

    ABC_CHECK_NULL_BUF(DID);
    ABC_CHECK_NULL_BUF(LPIN1);

    ABC_STR_NEW(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_PIN_PACK_GET_PATH);

    // create base64 versions
    ABC_CHECK_RET(ABC_CryptoBase64Encode(DID, &szDid_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LPIN1, &szLPIN1_Base64, pError));

    pJSON_Root = json_pack("{ss, ss}",
                    ABC_SERVER_JSON_DID_FIELD, szDid_Base64,
                    ABC_SERVER_JSON_LPIN1_FIELD, szLPIN1_Base64);

    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %s", szURL, szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %s", szResults);

    // Check the result
    ABC_CHECK_RET(ABC_URLCheckResults(szResults, &pJSON_Root, pError));

    // get the results field
    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_RESULTS_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_object(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON pin package results");

    // get the pin_package field
    pJSON_Value = json_object_get(pJSON_Value, JSON_ACCT_PIN_PACKAGE);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error pin package JSON results");

    // copy the value
    ABC_STRDUP(*szPinPackage, json_string_value(pJSON_Value));
exit:
    if (pJSON_Root)     json_decref(pJSON_Root);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szDid_Base64);
    ABC_FREE_STR(szLPIN1_Base64);
    ABC_FREE_STR(szResults);

    return cc;
}

/**
 * Uploads the pin package.
 *
 * @param L1            Login hash for the account
 * @param LP1           Login + Password hash
 * @param DID           Device Id
 * @param LPIN1         Hashed pin
 * @param szPinPackage  Pin package
 * @param szAli         auto-logout interval
 */
tABC_CC ABC_LoginServerUpdatePinPackage(tABC_U08Buf L1,
                                        tABC_U08Buf LP1,
                                        tABC_U08Buf DID,
                                        tABC_U08Buf LPIN1,
                                        char *szPinPackage,
                                        time_t ali,
                                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szURL          = NULL;
    char *szResults      = NULL;
    char *szPost         = NULL;
    char *szBase64_L1    = NULL;
    char *szBase64_LP1   = NULL;
    char *szBase64_LPIN1 = NULL;
    char *szBase64_DID   = NULL;
    json_t *pJSON_Root   = NULL;
    char szALI[DATETIME_LENGTH];

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL_BUF(LP1);
    ABC_CHECK_NULL_BUF(DID);
    ABC_CHECK_NULL_BUF(LPIN1);

    // create the URL
    ABC_STR_NEW(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_PIN_PACK_UPDATE_PATH);

    // create base64 versions
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1,  &szBase64_L1,  pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szBase64_LP1, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(DID, &szBase64_DID, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LPIN1, &szBase64_LPIN1, pError));

    // format the ali
    strftime(szALI, DATETIME_LENGTH, "%Y-%m-%dT%H:%M:%S", gmtime(&ali));

    // Encode those:
    pJSON_Root = json_pack("{ss, ss, ss, ss, ss, ss}",
                           ABC_SERVER_JSON_L1_FIELD, szBase64_L1,
                           ABC_SERVER_JSON_LP1_FIELD, szBase64_LP1,
                           ABC_SERVER_JSON_DID_FIELD, szBase64_DID,
                           ABC_SERVER_JSON_LPIN1_FIELD, szBase64_LPIN1,
                           JSON_ACCT_PIN_PACKAGE, szPinPackage,
                           ABC_SERVER_JSON_ALI_FIELD, szALI);
    ABC_CHECK_NULL(pJSON_Root);

    // create the post data
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
    ABC_FREE_STR(szBase64_L1);
    ABC_FREE_STR(szBase64_LP1);
    ABC_FREE_STR(szBase64_DID);
    ABC_FREE_STR(szBase64_LPIN1);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}


/**
 * Creates an git repo on the server.
 *
 * @param L1   Login hash for the account
 * @param LP1  Password hash for the account
 */
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
    ABC_STR_NEW(szURL, ABC_URL_MAX_PATH_LENGTH);
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
 * Upload files to auth server for debugging
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param szPassword    Password for the account associated with the settings
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_LoginServerUploadLogs(tABC_U08Buf L1,
                                  tABC_U08Buf LP1,
                                  tABC_SyncKeys *pKeys,
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
    void *aWatchData      = NULL;
    char *szWatchData_Hex = NULL;
    json_t *pJSON_Root    = NULL;
    tABC_U08Buf LogData   = ABC_BUF_NULL;
    tABC_U08Buf WatchData = ABC_BUF_NULL;
    size_t watcherSize    = 0;
    unsigned int nCount   = 0;
    tABC_WalletInfo **paWalletInfo = NULL;
    json_t *pJSON_array = NULL;

    // create the URL
    ABC_STR_NEW(szURL, ABC_URL_MAX_PATH_LENGTH);
    sprintf(szURL, "%s/%s", ABC_SERVER_ROOT, ABC_SERVER_DEBUG_PATH);

    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szLP1_Base64, pError));

    ABC_CHECK_RET(ABC_DebugLogFilename(&szLogFilename, pError);)
    ABC_CHECK_RET(ABC_FileIOReadFileStr(szLogFilename, &szLogData, pError));
    ABC_BUF_SET_PTR(LogData, (unsigned char *)szLogData, strlen(szLogData) + 1);
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LogData, &szLogData_Hex, pError));

    ABC_CHECK_RET(ABC_WalletGetWallets(pKeys, &paWalletInfo, &nCount, pError));
    pJSON_array = json_array();
    for (unsigned i = 0; i < nCount; ++i)
    {
        ABC_CHECK_RET(ABC_BridgeWatchPath(paWalletInfo[i]->szUUID,
                                          &szWatchFilename, pError));
        ABC_CHECK_RET(ABC_FileIOReadFile(szWatchFilename, &aWatchData, &watcherSize, pError));
        ABC_BUF_SET_PTR(WatchData, (unsigned char*)aWatchData, watcherSize);
        ABC_CHECK_RET(ABC_CryptoBase64Encode(WatchData, &szWatchData_Hex, pError));

        json_t *element = json_pack("s", szWatchData_Hex);
        json_array_append_new(pJSON_array, element);

        ABC_FREE_STR(szWatchFilename);
        ABC_FREE(aWatchData);
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
    ABC_FREE(aWatchData);
    ABC_FREE_STR(szWatchData_Hex);

    ABC_FreeWalletInfoArray(paWalletInfo, nCount);

    return cc;
}

} // namespace abcd
