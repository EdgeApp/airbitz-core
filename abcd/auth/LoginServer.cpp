/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginServer.hpp"
#include "AirbitzRequest.hpp"
#include "../login/Lobby.hpp"
#include "../login/Login.hpp"
#include "../login/LoginPackages.hpp"
#include "../crypto/Encoding.hpp"
#include "../json/JsonObject.hpp"
#include "../json/JsonArray.hpp"
#include "../util/Json.hpp"
#include "../util/Util.hpp"
#include <map>

// For debug upload:
#include "../account/Account.hpp"
#include "../bitcoin/WatcherBridge.hpp"
#include "../util/FileIO.hpp"
#include "../../src/LoginShim.hpp"

namespace abcd {

#define DATETIME_LENGTH 20

// Server strings:
#define JSON_ACCT_CARE_PACKAGE                  "care_package"
#define JSON_ACCT_LOGIN_PACKAGE                 "login_package"
#define JSON_ACCT_PIN_PACKAGE                   "pin_package"

#define ABC_SERVER_ROOT                     "https://app.auth.airbitz.co/api/v1"

#define ABC_SERVER_JSON_L1_FIELD            "l1"
#define ABC_SERVER_JSON_LP1_FIELD           "lp1"
#define ABC_SERVER_JSON_LRA1_FIELD          "lra1"
#define ABC_SERVER_JSON_NEW_LP1_FIELD       "new_lp1"
#define ABC_SERVER_JSON_NEW_LRA1_FIELD      "new_lra1"
#define ABC_SERVER_JSON_REPO_FIELD          "repo_account_key"
#define ABC_SERVER_JSON_CARE_PACKAGE_FIELD  "care_package"
#define ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD "login_package"
#define ABC_SERVER_JSON_DID_FIELD           "did"
#define ABC_SERVER_JSON_LPIN1_FIELD         "lpin1"
#define ABC_SERVER_JSON_ALI_FIELD           "ali"
#define ABC_SERVER_JSON_OTP_FIELD           "otp"
#define ABC_SERVER_JSON_OTP_SECRET_FIELD    "otp_secret"
#define ABC_SERVER_JSON_OTP_TIMEOUT         "otp_timeout"
#define ABC_SERVER_JSON_OTP_PENDING         "pending"
#define ABC_SERVER_JSON_OTP_ON              "on"
#define ABC_SERVER_JSON_OTP_RESET_AUTH      "otp_reset_auth"

#define ABC_SERVER_JSON_REPO_WALLET_FIELD       "repo_wallet_key"
#define ABC_SERVER_JSON_EREPO_WALLET_FIELD      "erepo_wallet_key"

typedef enum eABC_Server_Code
{
    ABC_Server_Code_Success = 0,
    ABC_Server_Code_Error = 1,
    ABC_Server_Code_AccountExists = 2,
    ABC_Server_Code_NoAccount = 3,
    ABC_Server_Code_InvalidPassword = 4,
    ABC_Server_Code_InvalidAnswers = 5,
    ABC_Server_Code_InvalidApiKey = 6,
    // Removed ABC_Server_Code_PinExpired = 7,
    ABC_Server_Code_InvalidOTP = 8,
    /** The endpoint is obsolete, and the app needs to be upgraded. */
    ABC_Server_Code_Obsolete = 1000
} tABC_Server_Code;

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

static tABC_CC ABC_LoginServerGetString(const Lobby &lobby, DataSlice LP1, DataSlice LRA1, const char *szURL, const char *szField, char **szResponse, tABC_Error *pError);
static tABC_CC ABC_LoginServerOtpRequest(const char *szUrl, const Lobby &lobby, DataSlice LP1, JsonPtr *results, tABC_Error *pError);
static tABC_CC ABC_WalletServerRepoPost(const Lobby &lobby, DataSlice LP1, const std::string &szWalletAcctKey, const char *szPath, tABC_Error *pError);

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
        {
            struct ResultJson: public JsonObject
            {
                ABC_JSON_CONSTRUCTORS(ResultJson, JsonObject)
                ABC_JSON_INTEGER(wait, "wait_seconds", 0)
            } resultJson(results());

            if (resultJson.waitOk())
                return ABC_ERROR(ABC_CC_InvalidPinWait, std::to_string(resultJson.wait()));
        }
        return ABC_ERROR(ABC_CC_BadPassword, "Invalid password on server");

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

    case ABC_Server_Code_Obsolete:
        return ABC_ERROR(ABC_CC_Obsolete, "Please upgrade Airbitz");

    case ABC_Server_Code_InvalidAnswers:
    case ABC_Server_Code_InvalidApiKey:
    default:
        return ABC_ERROR(ABC_CC_ServerError, message());
    }

    return Status();
}

Status
loginServerGetGeneral(JsonPtr &result)
{
    const auto url = ABC_SERVER_ROOT "/getinfo";
    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url));

    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    result = replyJson.results();
    return Status();
}

Status
loginServerGetQuestions(JsonPtr &result)
{
    const auto url = ABC_SERVER_ROOT "/questions";
    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url));

    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    result = replyJson.results();
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
                              DataSlice LP1,
                              const CarePackage &carePackage,
                              const LoginPackage &loginPackage,
                              const std::string &syncKey,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/account/create";
    HttpReply reply;
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    json_t *pJSON_Root = NULL;

    // create the post data
    pJSON_Root = json_pack("{ssssssssss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str(),
        ABC_SERVER_JSON_CARE_PACKAGE_FIELD, carePackage.encode().c_str(),
        ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, loginPackage.encode().c_str(),
        ABC_SERVER_JSON_REPO_FIELD, syncKey.c_str());
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost));

    // decode the result
    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

/**
 * Activate an account on the server.
 */
tABC_CC ABC_LoginServerActivate(const Login &login,
                                tABC_Error *pError)
{

    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/account/activate";
    HttpReply reply;
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    json_t *pJSON_Root = NULL;

    DataChunk authKey;
    ABC_CHECK_NEW(login.authKey(authKey));

    // create the post data
    pJSON_Root = json_pack("{ssss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(login.lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(authKey).c_str());
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost));

    // decode the result
    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

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

    const auto url = ABC_SERVER_ROOT "/account/available";
    HttpReply reply;
    ServerReplyJson replyJson;
    AccountAvailableJson json;

    // create the json
    ABC_CHECK_NEW(json.authIdSet(base64Encode(lobby.authId())));

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, json.encode()));

    // decode the result
    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

exit:
    return cc;
}

/**
 * Changes the password for an account on the server.
 */
tABC_CC ABC_LoginServerChangePassword(const Login &login,
                                      DataSlice newLP1,
                                      DataSlice newLRA1,
                                      const CarePackage &carePackage,
                                      const LoginPackage &loginPackage,
                                      tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/account/password/update";
    HttpReply reply;
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    json_t *pJSON_NewLRA1   = NULL;
    json_t *pJSON_Root = NULL;

    DataChunk authKey;
    ABC_CHECK_NEW(login.authKey(authKey));

    // Encode those:
    pJSON_Root = json_pack("{ss, ss, ss, ss, ss}",
        ABC_SERVER_JSON_L1_FIELD,      base64Encode(login.lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD,     base64Encode(authKey).c_str(),
        ABC_SERVER_JSON_NEW_LP1_FIELD, base64Encode(newLP1).c_str(),
        ABC_SERVER_JSON_CARE_PACKAGE_FIELD,  carePackage.encode().c_str(),
        ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, loginPackage.encode().c_str());

    // set up the recovery, if any:
    if (newLRA1.size())
    {
        pJSON_NewLRA1 = json_string(base64Encode(newLRA1).c_str());
        json_object_set(pJSON_Root, ABC_SERVER_JSON_NEW_LRA1_FIELD, pJSON_NewLRA1);
    }

    // create the post data
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost));

    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_NewLRA1)  json_decref(pJSON_NewLRA1);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

tABC_CC ABC_LoginServerGetCarePackage(const Lobby &lobby,
                                      CarePackage &result,
                                      tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/account/carepackage/get";
    char *szCarePackage = NULL;

    ABC_CHECK_RET(ABC_LoginServerGetString(lobby, DataChunk(), DataChunk(), url, JSON_ACCT_CARE_PACKAGE, &szCarePackage, pError));
    ABC_CHECK_NEW(result.decode(szCarePackage));

exit:
    ABC_FREE_STR(szCarePackage);

    return cc;
}

tABC_CC ABC_LoginServerGetLoginPackage(const Lobby &lobby,
                                       DataSlice LP1,
                                       DataSlice LRA1,
                                       LoginPackage &result,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/account/loginpackage/get";
    char *szLoginPackage = NULL;

    ABC_CHECK_RET(ABC_LoginServerGetString(lobby, LP1, LRA1, url, JSON_ACCT_LOGIN_PACKAGE, &szLoginPackage, pError));
    ABC_CHECK_NEW(result.decode(szLoginPackage));

exit:
    ABC_FREE_STR(szLoginPackage);

    return cc;
}

/**
 * Helper function for getting CarePackage or LoginPackage.
 */
static
tABC_CC ABC_LoginServerGetString(const Lobby &lobby, DataSlice LP1, DataSlice LRA1,
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
    ABC_CHECK_NEW(AirbitzRequest().post(reply, szURL, szPost));

    // Check the results, and store json if successful
    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

    // get the care package
    pJSON_Value = replyJson.results().get();
    ABC_CHECK_ASSERT((pJSON_Value && json_is_object(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON care package results");

    pJSON_Value = json_object_get(pJSON_Value, szField);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error care package JSON results");
    *szResponse = stringCopy(json_string_value(pJSON_Value));

exit:
    if (pJSON_Root)     json_decref(pJSON_Root);
    ABC_FREE_STR(szPost);

    return cc;
}

tABC_CC ABC_LoginServerGetPinPackage(DataSlice DID,
                                     DataSlice LPIN1,
                                     std::string &result,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/account/pinpackage/get";
    HttpReply reply;
    ServerReplyJson replyJson;
    json_t  *pJSON_Value    = NULL;
    json_t  *pJSON_Root     = NULL;
    char    *szPost         = NULL;

    pJSON_Root = json_pack("{ss, ss}",
        ABC_SERVER_JSON_DID_FIELD, base64Encode(DID).c_str(),
        ABC_SERVER_JSON_LPIN1_FIELD, base64Encode(LPIN1).c_str());

    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost));

    // Check the result
    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

    // get the results field
    pJSON_Value = replyJson.results().get();
    ABC_CHECK_ASSERT((pJSON_Value && json_is_object(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON pin package results");

    // get the pin_package field
    pJSON_Value = json_object_get(pJSON_Value, JSON_ACCT_PIN_PACKAGE);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error pin package JSON results");

    // copy the value
    result = json_string_value(pJSON_Value);

exit:
    if (pJSON_Root)     json_decref(pJSON_Root);
    ABC_FREE_STR(szPost);

    return cc;
}

/**
 * Uploads the pin package.
 *
 * @param DID           Device Id
 * @param LPIN1         Hashed pin
 * @param szPinPackage  Pin package
 * @param szAli         auto-logout interval
 */
tABC_CC ABC_LoginServerUpdatePinPackage(const Login &login,
                                        DataSlice DID,
                                        DataSlice LPIN1,
                                        const std::string &pinPackage,
                                        time_t ali,
                                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/account/pinpackage/update";
    HttpReply reply;
    ServerReplyJson replyJson;
    char *szPost         = NULL;
    json_t *pJSON_Root   = NULL;
    char szALI[DATETIME_LENGTH];

    DataChunk authKey;
    ABC_CHECK_NEW(login.authKey(authKey));

    // format the ali
    strftime(szALI, DATETIME_LENGTH, "%Y-%m-%dT%H:%M:%S", gmtime(&ali));

    // Encode those:
    pJSON_Root = json_pack("{ss, ss, ss, ss, ss, ss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(login.lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(authKey).c_str(),
        ABC_SERVER_JSON_DID_FIELD, base64Encode(DID).c_str(),
        ABC_SERVER_JSON_LPIN1_FIELD, base64Encode(LPIN1).c_str(),
        JSON_ACCT_PIN_PACKAGE, pinPackage.c_str(),
        ABC_SERVER_JSON_ALI_FIELD, szALI);

    // create the post data
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost));

    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

Status
LoginServerWalletCreate(const Login &login, const std::string &syncKey)
{
    DataChunk authKey;
    ABC_CHECK(login.authKey(authKey));
    ABC_CHECK_OLD(ABC_WalletServerRepoPost(login.lobby, authKey, syncKey,
        "wallet/create", &error));
    return Status();
}

Status
LoginServerWalletActivate(const Login &login, const std::string &syncKey)
{
    DataChunk authKey;
    ABC_CHECK(login.authKey(authKey));
    ABC_CHECK_OLD(ABC_WalletServerRepoPost(login.lobby, authKey, syncKey,
        "wallet/activate", &error));
    return Status();
}

static
tABC_CC ABC_WalletServerRepoPost(const Lobby &lobby,
                                 DataSlice LP1,
                                 const std::string &szWalletAcctKey,
                                 const char *szPath,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/" + std::string(szPath);
    HttpReply reply;
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    json_t *pJSON_Root = NULL;

    // create the post data
    pJSON_Root = json_pack("{ssssss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str(),
        ABC_SERVER_JSON_REPO_WALLET_FIELD, szWalletAcctKey.c_str());
    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost));

    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

/**
 * Enables 2 Factor authentication
 * @param timeout Amount of time needed for a reset to complete
 */
tABC_CC ABC_LoginServerOtpEnable(const Login &login,
                                 const std::string &otpToken,
                                 const long timeout,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/otp/on";
    HttpReply reply;
    ServerReplyJson replyJson;
    char *szPost    = NULL;
    json_t *pJSON_Root = NULL;

    DataChunk authKey;
    ABC_CHECK_NEW(login.authKey(authKey));

    // create the post data
    pJSON_Root = json_pack("{sssssssi}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(login.lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(authKey).c_str(),
        ABC_SERVER_JSON_OTP_SECRET_FIELD, otpToken.c_str(),
        ABC_SERVER_JSON_OTP_TIMEOUT, timeout);
    {
        auto key = login.lobby.otpKey();
        if (key)
            json_object_set_new(pJSON_Root, ABC_SERVER_JSON_OTP_FIELD, json_string(key->totp().c_str()));
    }

    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost));

    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);

    return cc;
}

tABC_CC ABC_LoginServerOtpRequest(const char *szUrl,
                                  const Lobby &lobby,
                                  DataSlice LP1,
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
    ABC_CHECK_NEW(AirbitzRequest().post(reply, szUrl, szPost));

    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());
    if (results)
        *results = replyJson.results();

exit:
    ABC_FREE_STR(szPost);
    if (pJSON_Root)     json_decref(pJSON_Root);
    return cc;
}

/**
 * Disable 2 Factor authentication
 */
tABC_CC ABC_LoginServerOtpDisable(const Login &login, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/otp/off";
    DataChunk authKey;
    ABC_CHECK_NEW(login.authKey(authKey));
    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url, login.lobby, authKey, NULL, pError));

exit:
    return cc;
}

tABC_CC ABC_LoginServerOtpStatus(const Login &login,
    bool &on, long &timeout, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/otp/status";
    json_t *pJSON_Value = NULL;
    JsonPtr reply;

    DataChunk authKey;
    ABC_CHECK_NEW(login.authKey(authKey));
    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url, login.lobby, authKey, &reply, pError));

    pJSON_Value = json_object_get(reply.get(), ABC_SERVER_JSON_OTP_ON);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_boolean(pJSON_Value)), ABC_CC_JSONError, "Error otp/on JSON");
    on = json_is_true(pJSON_Value);

    if (on)
    {
        pJSON_Value = json_object_get(reply.get(), ABC_SERVER_JSON_OTP_TIMEOUT);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error otp/timeout JSON");
        timeout = json_integer_value(pJSON_Value);
    }

exit:
    return cc;
}

/**
 * Request Reset 2 Factor authentication
 */
tABC_CC ABC_LoginServerOtpReset(const Lobby &lobby, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    const auto url = ABC_SERVER_ROOT "/otp/reset";
    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url, lobby, U08Buf(), NULL, pError));

exit:
    return cc;
}

tABC_CC ABC_LoginServerOtpPending(std::list<DataChunk> users, std::list<bool> &isPending, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/otp/pending/check";
    HttpReply reply;
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
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost));

    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

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

tABC_CC ABC_LoginServerOtpResetCancelPending(const Login &login, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    const auto url = ABC_SERVER_ROOT "/otp/pending/cancel";
    DataChunk authKey;
    ABC_CHECK_NEW(login.authKey(authKey));
    ABC_CHECK_RET(ABC_LoginServerOtpRequest(url, login.lobby, authKey, NULL, pError));

exit:
    return cc;
}


/**
 * Upload files to auth server for debugging
 */
tABC_CC ABC_LoginServerUploadLogs(const Account *account, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    const auto url = ABC_SERVER_ROOT "/account/debug";
    HttpReply reply;
    char *szPost          = NULL;
    char *szLogFilename   = NULL;
    json_t *pJSON_Root    = NULL;
    DataChunk logData;
    DataChunk watchData;
    json_t *pJSON_array = NULL;

    ABC_CHECK_RET(ABC_DebugLogFilename(&szLogFilename, pError);)
    ABC_CHECK_NEW(fileLoad(logData, szLogFilename));

    if (account)
    {
        DataChunk authKey;      // Unlocks the server
        ABC_CHECK_NEW(account->login.authKey(authKey));

        pJSON_array = json_array();

        auto ids = account->wallets.list();
        for (const auto &id: ids)
        {
            std::shared_ptr<Wallet> wallet;
            if (cacheWallet(wallet, nullptr, id.c_str()))
            {
                ABC_CHECK_NEW(fileLoad(watchData, watcherPath(*wallet)));
                json_array_append_new(pJSON_array,
                    json_string(base64Encode(watchData).c_str()));
            }
        }

        pJSON_Root = json_pack("{ss, ss, ss}",
            ABC_SERVER_JSON_L1_FIELD, base64Encode(account->login.lobby.authId()).c_str(),
            ABC_SERVER_JSON_LP1_FIELD, base64Encode(authKey).c_str(),
            "log", base64Encode(logData).c_str());
        json_object_set(pJSON_Root, "watchers", pJSON_array);
    }
    else
    {
        pJSON_Root = json_pack("{ss}", "log", base64Encode(logData).c_str());
    }

    szPost = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, szPost));

exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    if (pJSON_array) json_decref(pJSON_array);
    ABC_FREE_STR(szPost);
    ABC_FREE_STR(szLogFilename);

    return cc;
}

} // namespace abcd
