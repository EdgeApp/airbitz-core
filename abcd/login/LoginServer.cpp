/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginServer.hpp"
#include "ServerDefs.hpp"
#include "TwoFactor.hpp"
#include "../util/Json.hpp"
#include "../util/URL.hpp"
#include "../util/Util.hpp"
#include <list>
#include <map>

// For debug upload:
#include "../Bridge.hpp"
#include "../util/Crypto.hpp"
#include "../util/FileIO.hpp"

namespace abcd {

// Server strings:
#define JSON_ACCT_CARE_PACKAGE                  "care_package"
#define JSON_ACCT_LOGIN_PACKAGE                 "login_package"
#define JSON_ACCT_PIN_PACKAGE                   "pin_package"

#define DATETIME_LENGTH 20

static tABC_CC ABC_LoginServerGetString(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_U08Buf LRA1, const char *szURL, const char *szField, char **szResponse, tABC_Error *pError);
static tABC_CC checkResults(const char *szResults, json_t **ppJSON_Result, tABC_Error *pError);

/**
 * Creates an git repo on the server.
 *
 * @param L1   Login hash for the account
 * @param LP1  Password hash for the account
 */
static
tABC_CC ABC_WalletServerRepoPost(tABC_U08Buf L1, tABC_U08Buf LP1,
    const char *szWalletAcctKey, const char *szPath, tABC_Error *pError);

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
    ABC_CHECK_RET(checkResults(szResults, NULL, pError));
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
    ABC_CHECK_RET(checkResults(szResults, NULL, pError));
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

    ABC_CHECK_RET(checkResults(szResults, NULL, pError));
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
    tABC_U08Buf LP1_NULL = ABC_BUF_NULL; // Do not free
    tABC_U08Buf LRA1 = ABC_BUF_NULL; // Do not free
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
    char    *szToken        = NULL;

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
    ABC_CHECK_RET(ABC_TwoFactorGetToken(&szToken, pError));
    if (szToken)
    {
        json_object_set_new(pJSON_Root, ABC_SERVER_JSON_OTP_FIELD, json_string(szToken));
    }
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %s", szURL, szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(szURL, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %.50s", szResults);

    // Check the results, and store json if successful
    ABC_CHECK_RET(checkResults(szResults, &pJSON_Root, pError));

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
    ABC_FREE_STR(szToken);

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
    ABC_CHECK_RET(checkResults(szResults, &pJSON_Root, pError));

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

    ABC_CHECK_RET(checkResults(szResults, NULL, pError));
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

Status
LoginServerWalletCreate(tABC_U08Buf L1, tABC_U08Buf LP1, const char *syncKey)
{
    ABC_CHECK_OLD(ABC_WalletServerRepoPost(L1, LP1, syncKey,
        ABC_SERVER_WALLET_CREATE_PATH, &error));
    return Status();
}

Status
LoginServerWalletActivate(tABC_U08Buf L1, tABC_U08Buf LP1, const char *syncKey)
{
    ABC_CHECK_OLD(ABC_WalletServerRepoPost(L1, LP1, syncKey,
        ABC_SERVER_WALLET_ACTIVATE_PATH, &error));
    return Status();
}

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

    ABC_CHECK_RET(checkResults(szResults, NULL, pError));
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
 * Enables 2 Factor authentication
 *
 * @param L1   Login hash for the account
 * @param LP1  Password hash for the account
 * @param timeout Amount of time needed for a reset to complete
 */
tABC_CC ABC_LoginServerOtpEnable(tABC_U08Buf L1,
                                 tABC_U08Buf LP1,
                                 const char *szOtpSecret,
                                 const long timeout,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szResults = NULL;
    char *szPost    = NULL;
    char *szL1_Base64 = NULL;
    char *szLP1_Base64 = NULL;
    char *szToken = NULL;
    json_t *pJSON_Root = NULL;

    std::string url(ABC_SERVER_ROOT);
    url += "/otp/on";

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL_BUF(LP1);

    // create base64 versions of L1 and LP1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szLP1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{sssssssi}",
                        ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                        ABC_SERVER_JSON_LP1_FIELD, szLP1_Base64,
                        ABC_SERVER_JSON_OTP_SECRET_FIELD, szOtpSecret,
                        ABC_SERVER_JSON_OTP_TIMEOUT, timeout);
    ABC_CHECK_RET(ABC_TwoFactorGetToken(&szToken, pError));
    if (szToken)
    {
        json_object_set_new(pJSON_Root, ABC_SERVER_JSON_OTP_FIELD, json_string(szToken));
    }

    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %s", url.c_str(), szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(url.c_str(), szPost, &szResults, pError));
    ABC_DebugLog("Server results: %s", szResults);

    ABC_CHECK_RET(checkResults(szResults, NULL, pError));
exit:
    ABC_FREE_STR(szResults);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szL1_Base64);
    ABC_FREE_STR(szLP1_Base64);
    ABC_FREE_STR(szToken);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

tABC_CC ABC_LoginServerOtpRequest(const char *szUrl,
                                  tABC_U08Buf L1,
                                  tABC_U08Buf LP1,
                                  json_t **pJSON_Results,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szResults = NULL;
    char *szPost    = NULL;
    char *szL1_Base64 = NULL;
    char *szLP1_Base64 = NULL;
    char *szToken = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(L1);
    ABC_CHECK_NULL_BUF(LP1);

    // create base64 versions of L1 and LP1
    ABC_CHECK_RET(ABC_CryptoBase64Encode(L1, &szL1_Base64, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(LP1, &szLP1_Base64, pError));

    // create the post data
    pJSON_Root = json_pack("{ssss}",
                        ABC_SERVER_JSON_L1_FIELD, szL1_Base64,
                        ABC_SERVER_JSON_LP1_FIELD, szLP1_Base64);
    ABC_CHECK_RET(ABC_TwoFactorGetToken(&szToken, pError));
    if (szToken)
    {
        json_object_set_new(pJSON_Root, ABC_SERVER_JSON_OTP_FIELD, json_string(szToken));
    }
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %s", szUrl, szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(szUrl, szPost, &szResults, pError));
    ABC_DebugLog("Server results: %s", szResults);

    if (pJSON_Results) {
        ABC_CHECK_RET(checkResults(szResults, pJSON_Results, pError));
    } else {
        ABC_CHECK_RET(checkResults(szResults, NULL, pError));
    }
exit:
    ABC_FREE_STR(szResults);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szL1_Base64);
    ABC_FREE_STR(szLP1_Base64);
    ABC_FREE_STR(szToken);
    if (pJSON_Root)     json_decref(pJSON_Root);
    return cc;
}

/**
 * Disable 2 Factor authentication
 *
 * @param L1   Login hash for the account
 * @param LP1  Password hash for the account
 */
tABC_CC ABC_LoginServerOtpDisable(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string url(ABC_SERVER_ROOT);
    url += "/otp/off";
    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url.c_str(), L1, LP1, NULL, pError));

exit:
    return cc;
}

tABC_CC ABC_LoginServerOtpStatus(tABC_U08Buf L1, tABC_U08Buf LP1,
    bool *on, long *timeout, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Root = NULL;
    json_t *pJSON_Value = NULL;

    std::string url(ABC_SERVER_ROOT);
    url += "/otp/status";

    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url.c_str(), L1, LP1, &pJSON_Root, pError));

    pJSON_Root = json_object_get(pJSON_Root, ABC_SERVER_JSON_RESULTS_FIELD);
    ABC_CHECK_ASSERT((pJSON_Root && json_is_object(pJSON_Root)), ABC_CC_JSONError, "Error parsing server JSON care package results");

    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_OTP_ON);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_boolean(pJSON_Value)), ABC_CC_JSONError, "Error otp/on JSON");
    *on = json_is_true(pJSON_Value);

    if (*on)
    {
        pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_OTP_TIMEOUT);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error otp/timeout JSON");
        *timeout = json_integer_value(pJSON_Value);
    }
exit:
    if (pJSON_Root)      json_decref(pJSON_Root);
    if (pJSON_Value)     json_decref(pJSON_Value);

    return cc;
}

/**
 * Request Reset 2 Factor authentication
 *
 * @param L1   Login hash for the account
 * @param LP1  Password hash for the account
 */
tABC_CC ABC_LoginServerOtpReset(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::string url(ABC_SERVER_ROOT);
    url += "/otp/reset";
    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url.c_str(), L1, LP1, NULL, pError));

exit:
    return cc;
}

tABC_CC ABC_LoginServerOtpPending(std::vector<tABC_U08Buf> users, std::vector<bool>& isPending, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Root = NULL;
    json_t *pJSON_Row = NULL;
    json_t *pJSON_Value = NULL;
    size_t rows = 0;
    char *szToken = NULL;
    char *szResults = NULL;
    char *szPost = NULL;
    std::map<std::string, bool> userMap;
    std::list<std::string> usersEncoded;

    std::string url(ABC_SERVER_ROOT);
    url += "/otp/pending/check";

    std::string param;
    for (tABC_U08Buf u : users)
    {
        char *szL1 = NULL;
        ABC_CHECK_RET(ABC_CryptoBase64Encode(u, &szL1, pError));

        std::string username(szL1);
        param += (username + ",");
        userMap[username] = false;
        usersEncoded.push_back(username);

        ABC_FREE_STR(szL1);
    }

    // create the post data
    pJSON_Root = json_pack("{ss}", "l1s", param.c_str());
    ABC_CHECK_RET(ABC_TwoFactorGetToken(&szToken, pError));
    if (szToken)
    {
        json_object_set_new(pJSON_Root, ABC_SERVER_JSON_OTP_FIELD, json_string(szToken));
    }
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);
    json_decref(pJSON_Root);
    pJSON_Root = NULL;
    ABC_DebugLog("Server URL: %s, Data: %s", url.c_str(), szPost);

    // send the command
    ABC_CHECK_RET(ABC_URLPostString(url.c_str(), szPost, &szResults, pError));
    ABC_DebugLog("Server results: %s", szResults);
    ABC_CHECK_RET(checkResults(szResults, &pJSON_Root, pError));

    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_RESULTS_FIELD);
    if (pJSON_Value)
    {
        ABC_CHECK_ASSERT((pJSON_Value && json_is_array(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON care package results");

        rows = (size_t) json_array_size(pJSON_Value);
        for (unsigned i = 0; i < rows; i++)
        {
            json_t *pJSON_Row = json_array_get(pJSON_Value, i);
            ABC_CHECK_ASSERT((pJSON_Row && json_is_object(pJSON_Row)), ABC_CC_JSONError, "Error parsing JSON array element object");

            pJSON_Value = json_object_get(pJSON_Row, "login");
            ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error otp/pending/login JSON");
            std::string username(json_string_value(pJSON_Value));

            pJSON_Value = json_object_get(pJSON_Row, ABC_SERVER_JSON_OTP_PENDING);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_boolean(pJSON_Value)), ABC_CC_JSONError, "Error otp/pending/pending JSON");
            if (json_is_true(pJSON_Value))
            {
                userMap[username] = json_is_true(pJSON_Value);;
            }
        }
    }
    isPending.clear();
    for (auto &username: usersEncoded) {
        isPending.push_back(userMap[username]);
    }
exit:
    ABC_FREE_STR(szToken);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szResults);

    if (pJSON_Root)      json_decref(pJSON_Root);
    if (pJSON_Row)       json_decref(pJSON_Row);
    if (pJSON_Value)     json_decref(pJSON_Value);

    return cc;
}

/**
 * Request Reset 2 Factor authentication
 *
 * @param L1   Login hash for the account
 * @param LP1  Password hash for the account
 */
tABC_CC ABC_LoginServerOtpResetCancelPending(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::string url(ABC_SERVER_ROOT);
    url += "/otp/pending/cancel";
    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url.c_str(), L1, LP1, NULL, pError));

exit:
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
    tABC_U08Buf LogData   = ABC_BUF_NULL; // Do not free
    tABC_U08Buf WatchData = ABC_BUF_NULL; // Do not free
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

/**
 * Makes a URL post request and returns results in a string.
 * @param szURL         The request URL.
 * @param szPostData    The data to be posted in the request
 * @param pszResults    The location to store the allocated string with results.
 *                      The caller is responsible for free'ing this.
 */
static
tABC_CC checkResults(const char *szResults, json_t **ppJSON_Result, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int statusCode = 0;
    json_error_t error;
    json_t *pJSON_Root = NULL;
    json_t *pJSON_Value = NULL;

    pJSON_Root = json_loads(szResults, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing server JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // get the status code
    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_STATUS_CODE_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_number(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON status code");
    statusCode = (int) json_integer_value(pJSON_Value);

    // if there was a failure
    if (ABC_Server_Code_Success != statusCode)
    {
        if (ABC_Server_Code_AccountExists == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_AccountAlreadyExists, "Account already exists on server");
        }
        else if (ABC_Server_Code_NoAccount == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "Account does not exist on server");
        }
        else if (ABC_Server_Code_InvalidPassword == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_BadPassword, "Invalid password on server");
        }
        else if (ABC_Server_Code_PinExpired == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_PinExpired, "Invalid password on server");
        }
        else if (ABC_Server_Code_InvalidOTP == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_InvalidOTP, "Invalid OTP");
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
    if (ppJSON_Result)
    {
        *ppJSON_Result = pJSON_Root;
        pJSON_Root = NULL;
    }
exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    return cc;
}

} // namespace abcd
