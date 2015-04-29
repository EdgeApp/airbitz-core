/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginServer.hpp"
#include "ServerDefs.hpp"
#include "Lobby.hpp"
#include "Login.hpp"
#include "../crypto/Encoding.hpp"
#include "../http/AirbitzRequest.hpp"
#include "../json/JsonObject.hpp"
#include "../json/JsonArray.hpp"
#include "../util/Json.hpp"
#include "../util/Util.hpp"
#include <map>

// For debug upload:
#include "../account/Account.hpp"
#include "../bitcoin/WatcherBridge.hpp"
#include "../util/FileIO.hpp"

namespace abcd {

// Server strings:
#define JSON_ACCT_CARE_PACKAGE                  "care_package"
#define JSON_ACCT_LOGIN_PACKAGE                 "login_package"
#define JSON_ACCT_PIN_PACKAGE                   "pin_package"

#define DATETIME_LENGTH 20

struct AccountAvailableJson: public JsonObject
{
    ABC_JSON_STRING(authId, "l1", nullptr)
};

/**
 * The common format shared by server reply messages.
 */
struct ServerReplyJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(ServerReplyJson, JsonObject)

    ABC_JSON_INTEGER(code, "status_code", ABC_Server_Code_Success)
    ABC_JSON_STRING(message, "message", "<no server message>")
    ABC_JSON_VALUE(results, "results", JsonPtr)

    /**
     * Checks the server status code for errors.
     */
    Status
    ok();
};

static std::string gOtpResetAuth;
std::string gOtpResetDate;

static tABC_CC ABC_LoginServerGetString(const Lobby &lobby, tABC_U08Buf LP1, tABC_U08Buf LRA1, const char *szURL, const char *szField, char **szResponse, tABC_Error *pError);
static tABC_CC ABC_LoginServerOtpRequest(const char *szUrl, const Lobby &lobby, tABC_U08Buf LP1, JsonPtr *results, tABC_Error *pError);
static tABC_CC ABC_WalletServerRepoPost(const Lobby &lobby, tABC_U08Buf LP1, const char *szWalletAcctKey, const char *szPath, tABC_Error *pError);

Status
ServerReplyJson::ok()
{
    switch (code())
    {
    case ABC_Server_Code_Success:
        break;

    case ABC_Server_Code_AccountExists:
        return ABC_ERROR(ABC_CC_AccountAlreadyExists, "Account already exists on server");

    case ABC_Server_Code_NoAccount:
        return ABC_ERROR(ABC_CC_AccountDoesNotExist, "Account does not exist on server");

    case ABC_Server_Code_InvalidPassword:
        return ABC_ERROR(ABC_CC_BadPassword, "Invalid password on server");

    case ABC_Server_Code_PinExpired:
        return ABC_ERROR(ABC_CC_PinExpired, "PIN expired");

    case ABC_Server_Code_InvalidOTP:
        {
            struct ResultJson: public JsonObject
            {
                ABC_JSON_CONSTRUCTORS(ResultJson, JsonObject)
                ABC_JSON_STRING(resetAuth, "otp_reset_auth", nullptr)
                ABC_JSON_STRING(resetDate, "otp_timeout_date", nullptr)
            } resultJson(results());

            if (resultJson.resetAuthOk())
                gOtpResetAuth = resultJson.resetAuth();
            if (resultJson.resetDateOk())
                gOtpResetDate = resultJson.resetDate();
        }
        return ABC_ERROR(ABC_CC_InvalidOTP, "Invalid OTP");

    case ABC_Server_Code_InvalidAnswers:
    case ABC_Server_Code_InvalidApiKey:
    default:
        return ABC_ERROR(ABC_CC_ServerError, message());
    }

    return Status();
}

/**
 * Creates an account on the server.
 *
 * This function sends information to the server to create an account.
 * If the account was created, ABC_CC_Ok is returned.
 * If the account already exists, ABC_CC_AccountAlreadyExists is returned.
 *
 * @param LP1  Password hash for the account
 */
tABC_CC ABC_LoginServerCreate(const Lobby &lobby,
                              tABC_U08Buf LP1,
                              const CarePackage &carePackage,
                              const LoginPackage &loginPackage,
                              const char *szRepoAcctKey,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    std::string url = ABC_SERVER_ROOT "/" ABC_SERVER_ACCOUNT_CREATE_PATH;
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    std::string carePackageStr;
    std::string loginPackageStr;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(LP1);

    ABC_CHECK_NEW(carePackage.encode(carePackageStr), pError);
    ABC_CHECK_NEW(loginPackage.encode(loginPackageStr), pError);

    // create the post data
    pJSON_Root = json_pack("{ssssssssss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str(),
        ABC_SERVER_JSON_CARE_PACKAGE_FIELD, carePackageStr.c_str(),
        ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, loginPackageStr.c_str(),
        ABC_SERVER_JSON_REPO_FIELD, szRepoAcctKey);
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost), pError);

    // decode the result
    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

/**
 * Activate an account on the server.
 *
 * @param LP1  Password hash for the account
 */
tABC_CC ABC_LoginServerActivate(const Lobby &lobby,
                                tABC_U08Buf LP1,
                                tABC_Error *pError)
{

    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    std::string url = ABC_SERVER_ROOT "/" ABC_SERVER_ACCOUNT_ACTIVATE;
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    json_t *pJSON_Root = NULL;

    // create the post data
    pJSON_Root = json_pack("{ssss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str());
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost), pError);

    // decode the result
    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

/**
 * Queries the server to determine if a username is available.
 */
tABC_CC ABC_LoginServerAvailable(const Lobby &lobby,
                                 tABC_Error *pError)
{

    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    std::string url = ABC_SERVER_ROOT "/" ABC_SERVER_ACCOUNT_AVAILABLE;
    ServerReplyJson replyJson;
    std::string get;
    AccountAvailableJson json;

    // create the json
    ABC_CHECK_NEW(json.authIdSet(base64Encode(lobby.authId()).c_str()), pError);
    ABC_CHECK_NEW(json.encode(get), pError);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, get), pError);

    // decode the result
    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);

exit:
    return cc;
}

/**
 * Changes the password for an account on the server.
 *
 * This function sends information to the server to change the password for an account.
 * Either the old LP1 or LRA1 can be used for authentication.
 *
 * @param oldLP1 Old password hash for the account (if this is empty, LRA1 is used instead)
 * @param LRA1   LRA1 for the account (used if oldP1 is empty)
 */
tABC_CC ABC_LoginServerChangePassword(const Lobby &lobby,
                                      tABC_U08Buf oldLP1,
                                      tABC_U08Buf newLP1,
                                      tABC_U08Buf newLRA1,
                                      const CarePackage &carePackage,
                                      const LoginPackage &loginPackage,
                                      tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    std::string url = ABC_SERVER_ROOT "/" ABC_SERVER_CHANGE_PASSWORD_PATH;
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    std::string carePackageStr;
    std::string loginPackageStr;
    json_t *pJSON_OldLRA1   = NULL;
    json_t *pJSON_NewLRA1   = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(oldLP1);
    ABC_CHECK_NULL_BUF(newLP1);

    ABC_CHECK_NEW(carePackage.encode(carePackageStr), pError);
    ABC_CHECK_NEW(loginPackage.encode(loginPackageStr), pError);

    // Encode those:
    pJSON_Root = json_pack("{ss, ss, ss, ss, ss}",
        ABC_SERVER_JSON_L1_FIELD,      base64Encode(lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD,     base64Encode(oldLP1).c_str(),
        ABC_SERVER_JSON_NEW_LP1_FIELD, base64Encode(newLP1).c_str(),
        ABC_SERVER_JSON_CARE_PACKAGE_FIELD,  carePackageStr.c_str(),
        ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, loginPackageStr.c_str());

    // set up the recovery, if any:
    if (newLRA1.size())
    {
        pJSON_NewLRA1 = json_string(base64Encode(newLRA1).c_str());
        json_object_set(pJSON_Root, ABC_SERVER_JSON_NEW_LRA1_FIELD, pJSON_NewLRA1);
    }

    // create the post data
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost), pError);

    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_OldLRA1)  json_decref(pJSON_OldLRA1);
    if (pJSON_NewLRA1)  json_decref(pJSON_NewLRA1);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

tABC_CC ABC_LoginServerGetCarePackage(const Lobby &lobby,
                                      CarePackage &result,
                                      tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string url = ABC_SERVER_ROOT "/" ABC_SERVER_GET_CARE_PACKAGE_PATH;
    char *szCarePackage = NULL;

    ABC_CHECK_RET(ABC_LoginServerGetString(lobby, U08Buf(), U08Buf(), url.c_str(), JSON_ACCT_CARE_PACKAGE, &szCarePackage, pError));
    ABC_CHECK_NEW(result.decode(szCarePackage), pError);

exit:
    ABC_FREE_STR(szCarePackage);

    return cc;
}

tABC_CC ABC_LoginServerGetLoginPackage(const Lobby &lobby,
                                       tABC_U08Buf LP1,
                                       tABC_U08Buf LRA1,
                                       LoginPackage &result,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string url = ABC_SERVER_ROOT "/" ABC_SERVER_LOGIN_PACK_GET_PATH;
    char *szLoginPackage = NULL;

    ABC_CHECK_RET(ABC_LoginServerGetString(lobby, LP1, LRA1, url.c_str(), JSON_ACCT_LOGIN_PACKAGE, &szLoginPackage, pError));
    ABC_CHECK_NEW(result.decode(szLoginPackage), pError);

exit:
    ABC_FREE_STR(szLoginPackage);

    return cc;
}

/**
 * Helper function for getting CarePackage or LoginPackage.
 */
static
tABC_CC ABC_LoginServerGetString(const Lobby &lobby, tABC_U08Buf LP1, tABC_U08Buf LRA1,
                                 const char *szURL, const char *szField, char **szResponse, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    ServerReplyJson replyJson;
    json_t  *pJSON_Value    = NULL;
    json_t  *pJSON_Root     = NULL;
    char    *szPost         = NULL;

    // create the post data with or without LP1
    if (LP1.data() == NULL && LRA1.data() == NULL)
    {
        pJSON_Root = json_pack("{ss}",
            ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str());
    }
    else
    {
        if (LP1.data() == NULL)
        {
            pJSON_Root = json_pack("{ssss}",
                ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
                ABC_SERVER_JSON_LRA1_FIELD, base64Encode(LRA1).c_str());
        }
        else
        {
            pJSON_Root = json_pack("{ssss}",
                ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
                ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str());
        }
    }
    {
        auto key = lobby.otpKey();
        if (key)
            json_object_set_new(pJSON_Root, ABC_SERVER_JSON_OTP_FIELD, json_string(key->totp().c_str()));
    }
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, szURL, szPost), pError);

    // Check the results, and store json if successful
    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);

    // get the care package
    pJSON_Value = replyJson.results().get();
    ABC_CHECK_ASSERT((pJSON_Value && json_is_object(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON care package results");

    pJSON_Value = json_object_get(pJSON_Value, szField);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error care package JSON results");
    ABC_STRDUP(*szResponse, json_string_value(pJSON_Value));

exit:
    if (pJSON_Root)     json_decref(pJSON_Root);
    ABC_FREE_STR(szPost);

    return cc;
}

tABC_CC ABC_LoginServerGetPinPackage(tABC_U08Buf DID,
                                     tABC_U08Buf LPIN1,
                                     char **szPinPackage,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    std::string url = ABC_SERVER_ROOT "/" ABC_SERVER_PIN_PACK_GET_PATH;
    ServerReplyJson replyJson;
    json_t  *pJSON_Value    = NULL;
    json_t  *pJSON_Root     = NULL;
    char    *szPost         = NULL;

    ABC_CHECK_NULL_BUF(DID);
    ABC_CHECK_NULL_BUF(LPIN1);

    pJSON_Root = json_pack("{ss, ss}",
        ABC_SERVER_JSON_DID_FIELD, base64Encode(DID).c_str(),
        ABC_SERVER_JSON_LPIN1_FIELD, base64Encode(LPIN1).c_str());

    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost), pError);

    // Check the result
    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);

    // get the results field
    pJSON_Value = replyJson.results().get();
    ABC_CHECK_ASSERT((pJSON_Value && json_is_object(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON pin package results");

    // get the pin_package field
    pJSON_Value = json_object_get(pJSON_Value, JSON_ACCT_PIN_PACKAGE);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error pin package JSON results");

    // copy the value
    ABC_STRDUP(*szPinPackage, json_string_value(pJSON_Value));

exit:
    if (pJSON_Root)     json_decref(pJSON_Root);
    ABC_FREE_STR(szPost);

    return cc;
}

/**
 * Uploads the pin package.
 *
 * @param LP1           Login + Password hash
 * @param DID           Device Id
 * @param LPIN1         Hashed pin
 * @param szPinPackage  Pin package
 * @param szAli         auto-logout interval
 */
tABC_CC ABC_LoginServerUpdatePinPackage(const Lobby &lobby,
                                        tABC_U08Buf LP1,
                                        tABC_U08Buf DID,
                                        tABC_U08Buf LPIN1,
                                        const std::string &pinPackage,
                                        time_t ali,
                                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    std::string url = ABC_SERVER_ROOT "/" ABC_SERVER_PIN_PACK_UPDATE_PATH;
    ServerReplyJson replyJson;
    char *szPost         = NULL;
    json_t *pJSON_Root   = NULL;
    char szALI[DATETIME_LENGTH];

    ABC_CHECK_NULL_BUF(LP1);
    ABC_CHECK_NULL_BUF(DID);
    ABC_CHECK_NULL_BUF(LPIN1);

    // format the ali
    strftime(szALI, DATETIME_LENGTH, "%Y-%m-%dT%H:%M:%S", gmtime(&ali));

    // Encode those:
    pJSON_Root = json_pack("{ss, ss, ss, ss, ss, ss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str(),
        ABC_SERVER_JSON_DID_FIELD, base64Encode(DID).c_str(),
        ABC_SERVER_JSON_LPIN1_FIELD, base64Encode(LPIN1).c_str(),
        JSON_ACCT_PIN_PACKAGE, pinPackage.c_str(),
        ABC_SERVER_JSON_ALI_FIELD, szALI);

    // create the post data
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost), pError);

    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

Status
LoginServerWalletCreate(const Lobby &lobby, tABC_U08Buf LP1, const char *syncKey)
{
    ABC_CHECK_OLD(ABC_WalletServerRepoPost(lobby, LP1, syncKey,
        ABC_SERVER_WALLET_CREATE_PATH, &error));
    return Status();
}

Status
LoginServerWalletActivate(const Lobby &lobby, tABC_U08Buf LP1, const char *syncKey)
{
    ABC_CHECK_OLD(ABC_WalletServerRepoPost(lobby, LP1, syncKey,
        ABC_SERVER_WALLET_ACTIVATE_PATH, &error));
    return Status();
}

static
tABC_CC ABC_WalletServerRepoPost(const Lobby &lobby,
                                 tABC_U08Buf LP1,
                                 const char *szWalletAcctKey,
                                 const char *szPath,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    std::string url = ABC_SERVER_ROOT "/" + std::string(szPath);
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(LP1);

    // create the post data
    pJSON_Root = json_pack("{ssssss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str(),
        ABC_SERVER_JSON_REPO_WALLET_FIELD, szWalletAcctKey);
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost), pError);

    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

/**
 * Enables 2 Factor authentication
 *
 * @param LP1  Password hash for the account
 * @param timeout Amount of time needed for a reset to complete
 */
tABC_CC ABC_LoginServerOtpEnable(const Lobby &lobby,
                                 tABC_U08Buf LP1,
                                 const char *szOtpSecret,
                                 const long timeout,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    std::string url = ABC_SERVER_ROOT "/otp/on";
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL_BUF(LP1);

    // create the post data
    pJSON_Root = json_pack("{sssssssi}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str(),
        ABC_SERVER_JSON_OTP_SECRET_FIELD, szOtpSecret,
        ABC_SERVER_JSON_OTP_TIMEOUT, timeout);
    {
        auto key = lobby.otpKey();
        if (key)
            json_object_set_new(pJSON_Root, ABC_SERVER_JSON_OTP_FIELD, json_string(key->totp().c_str()));
    }

    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost), pError);

    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

tABC_CC ABC_LoginServerOtpRequest(const char *szUrl,
                                  const Lobby &lobby,
                                  tABC_U08Buf LP1,
                                  JsonPtr *results,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    json_t *pJSON_Root = NULL;

    // create the post data
    pJSON_Root = json_pack("{ss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str());
    // If there is a LP1 provided
    if (LP1.data())
    {
        json_object_set_new(pJSON_Root, ABC_SERVER_JSON_LP1_FIELD,
            json_string(base64Encode(LP1).c_str()));
    }
    {
        auto key = lobby.otpKey();
        if (key)
            json_object_set_new(pJSON_Root, ABC_SERVER_JSON_OTP_FIELD, json_string(key->totp().c_str()));
    }
    json_object_set_new(pJSON_Root, ABC_SERVER_JSON_OTP_RESET_AUTH, json_string(gOtpResetAuth.c_str()));
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, szUrl, szPost), pError);

    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);
    if (results)
        *results = replyJson.results();

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);
    return cc;
}

/**
 * Disable 2 Factor authentication
 *
 * @param LP1  Password hash for the account
 */
tABC_CC ABC_LoginServerOtpDisable(const Lobby &lobby, tABC_U08Buf LP1, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string url = ABC_SERVER_ROOT "/otp/off";
    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url.c_str(), lobby, LP1, NULL, pError));

exit:
    return cc;
}

tABC_CC ABC_LoginServerOtpStatus(const Lobby &lobby, tABC_U08Buf LP1,
    bool *on, long *timeout, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Value = NULL;
    JsonPtr reply;

    std::string url = ABC_SERVER_ROOT "/otp/status";

    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url.c_str(), lobby, LP1, &reply, pError));

    pJSON_Value = json_object_get(reply.get(), ABC_SERVER_JSON_OTP_ON);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_boolean(pJSON_Value)), ABC_CC_JSONError, "Error otp/on JSON");
    *on = json_is_true(pJSON_Value);

    if (*on)
    {
        pJSON_Value = json_object_get(reply.get(), ABC_SERVER_JSON_OTP_TIMEOUT);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error otp/timeout JSON");
        *timeout = json_integer_value(pJSON_Value);
    }

exit:
    return cc;
}

/**
 * Request Reset 2 Factor authentication
 *
 * @param LP1  Password hash for the account
 */
tABC_CC ABC_LoginServerOtpReset(const Lobby &lobby, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::string url = ABC_SERVER_ROOT "/otp/reset";
    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url.c_str(), lobby, U08Buf(), NULL, pError));

exit:
    return cc;
}

tABC_CC ABC_LoginServerOtpPending(std::list<DataChunk> users, std::list<bool> &isPending, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    HttpReply reply;
    std::string url = ABC_SERVER_ROOT "/otp/pending/check";
    ServerReplyJson replyJson;
    JsonArray arrayJson;
    json_t *pJSON_Root = NULL;
    char *szPost = NULL;
    std::map<std::string, bool> userMap;
    std::list<std::string> usersEncoded;

    std::string param;
    for (const auto &u : users)
    {
        std::string username = base64Encode(u);
        param += (username + ",");
        userMap[username] = false;
        usersEncoded.push_back(username);
    }

    // create the post data
    pJSON_Root = json_pack("{ss}", "l1s", param.c_str());
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost), pError);

    ABC_CHECK_NEW(replyJson.decode(reply.body), pError);
    ABC_CHECK_NEW(replyJson.ok(), pError);

    arrayJson = replyJson.results();
    if (arrayJson)
    {
        size_t rows = arrayJson.size();
        for (size_t i = 0; i < rows; i++)
        {
            json_t *pJSON_Row = arrayJson[i].get();
            ABC_CHECK_ASSERT((pJSON_Row && json_is_object(pJSON_Row)), ABC_CC_JSONError, "Error parsing JSON array element object");

            json_t *pJSON_Value = json_object_get(pJSON_Row, "login");
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
    ABC_FREE_STR(szPost);
    if (pJSON_Root)      json_decref(pJSON_Root);

    return cc;
}

/**
 * Request Reset 2 Factor authentication
 *
 * @param LP1  Password hash for the account
 */
tABC_CC ABC_LoginServerOtpResetCancelPending(const Lobby &lobby, tABC_U08Buf LP1, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::string url = ABC_SERVER_ROOT "/otp/pending/cancel";
    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url.c_str(), lobby, LP1, NULL, pError));

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
tABC_CC ABC_LoginServerUploadLogs(const Account &account,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    HttpReply reply;
    std::string url = ABC_SERVER_ROOT "/" ABC_SERVER_DEBUG_PATH;
    char *szPost          = NULL;
    char *szLogFilename   = NULL;
    char *szWatchFilename = NULL;
    json_t *pJSON_Root    = NULL;
    DataChunk logData;
    DataChunk watchData;
    auto uuids = account.wallets.list();
    json_t *pJSON_array = NULL;

    AutoU08Buf LP1;
    ABC_CHECK_RET(ABC_LoginGetServerKey(account.login(), &LP1, pError));

    ABC_CHECK_RET(ABC_DebugLogFilename(&szLogFilename, pError);)
    ABC_CHECK_NEW(fileLoad(logData, szLogFilename), pError);

    pJSON_array = json_array();
    for (const auto &i: uuids)
    {
        ABC_CHECK_RET(ABC_BridgeWatchPath(i.id.c_str(),
                                          &szWatchFilename, pError));
        ABC_CHECK_NEW(fileLoad(watchData, szWatchFilename), pError);

        json_array_append_new(pJSON_array,
            json_string(base64Encode(watchData).c_str()));

        ABC_FREE_STR(szWatchFilename);
    }

    pJSON_Root = json_pack("{ss, ss, ss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(account.login().lobby().authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str(),
        "log", base64Encode(logData).c_str());
    json_object_set(pJSON_Root, "watchers", pJSON_array);

    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost), pError);

exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    if (pJSON_array) json_decref(pJSON_array);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szLogFilename);

    ABC_FREE_STR(szWatchFilename);

    return cc;
}

} // namespace abcd
