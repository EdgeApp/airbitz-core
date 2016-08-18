/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginServer.hpp"
#include "AirbitzRequest.hpp"
#include "AuthJson.hpp"
#include "LoginJson.hpp"
#include "../Login.hpp"
#include "../LoginPackages.hpp"
#include "../LoginStore.hpp"
#include "../../crypto/Encoding.hpp"
#include "../../json/JsonObject.hpp"
#include "../../json/JsonArray.hpp"
#include "../../util/Debug.hpp"
#include <map>

// For debug upload:
#include "../../WalletPaths.hpp"
#include "../../account/Account.hpp"
#include "../../util/FileIO.hpp"

namespace abcd {

#define DATETIME_LENGTH 20

// Server strings:
#define ABC_SERVER_ROOT                     "https://test-auth.airbitz.co/api"

#define ABC_SERVER_JSON_NEW_LP1_FIELD       "new_lp1"
#define ABC_SERVER_JSON_NEW_LRA1_FIELD      "new_lra1"
#define ABC_SERVER_JSON_REPO_FIELD          "repo_account_key"
#define ABC_SERVER_JSON_CARE_PACKAGE_FIELD  "care_package"
#define ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD "login_package"
#define ABC_SERVER_JSON_DID_FIELD           "did"
#define ABC_SERVER_JSON_LPIN1_FIELD         "lpin1"
#define ABC_SERVER_JSON_ALI_FIELD           "ali"
#define ABC_SERVER_JSON_OTP_SECRET_FIELD    "otp_secret"
#define ABC_SERVER_JSON_OTP_TIMEOUT         "otp_timeout"
#define ABC_SERVER_JSON_OTP_PENDING         "pending"
#define ABC_SERVER_JSON_REPO_WALLET_FIELD       "repo_wallet_key"
#define JSON_ACCT_PIN_PACKAGE                   "pin_package"

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
    decode(const HttpReply &reply, AuthError *authError=nullptr);
};

Status
ServerReplyJson::decode(const HttpReply &reply, AuthError *authError)
{
    ABC_CHECK(JsonObject::decode(reply.body));

    // First check the body for a descriptive error code:
    switch (code())
    {
    case ABC_Server_Code_Success:
        break;

    case ABC_Server_Code_AccountExists:
        return ABC_ERROR(ABC_CC_AccountAlreadyExists,
                         "Account already exists on server");

    case ABC_Server_Code_NoAccount:
        return ABC_ERROR(ABC_CC_AccountDoesNotExist,
                         "Account does not exist on server");

    case ABC_Server_Code_InvalidPassword:
    {
        struct ResultJson: public JsonObject
        {
            ABC_JSON_CONSTRUCTORS(ResultJson, JsonObject)
            ABC_JSON_INTEGER(wait, "wait_seconds", 0)
        } resultJson(results());

        if (authError)
            authError->pinWait = resultJson.wait();
        if (resultJson.waitOk())
            return ABC_ERROR(ABC_CC_InvalidPinWait,
                             std::to_string(resultJson.wait()));
    }
    return ABC_ERROR(ABC_CC_BadPassword, "Invalid password on server");

    case ABC_Server_Code_InvalidOTP:
    {
        struct ResultJson: public JsonObject
        {
            ABC_JSON_CONSTRUCTORS(ResultJson, JsonObject)
            ABC_JSON_STRING(resetToken, "otp_reset_auth", "")
            ABC_JSON_STRING(resetDate, "otp_timeout_date", "")
        } resultJson(results());

        if (authError)
        {
            authError->otpToken = resultJson.resetToken();
            authError->otpDate = resultJson.resetDate();
        }
    }
    return ABC_ERROR(ABC_CC_InvalidOTP, "Invalid OTP");

    case ABC_Server_Code_Obsolete:
        return ABC_ERROR(ABC_CC_Obsolete, "Please upgrade Airbitz");

    case ABC_Server_Code_InvalidAnswers:
    case ABC_Server_Code_InvalidApiKey:
    default:
        return ABC_ERROR(ABC_CC_ServerError, message());
    }

    // Also check the HTTP status code:
    ABC_CHECK(reply.codeOk());

    return Status();
}

/**
 * The common format shared by outgoing authentication information.
 */
struct ServerRequestJson:
    public JsonObject
{
    ABC_JSON_STRING(userId, "l1", nullptr)
    ABC_JSON_STRING(passwordAuth, "lp1", nullptr)
    ABC_JSON_STRING(recoveryAuth, "lra1", nullptr)
    ABC_JSON_STRING(otp, "otp", nullptr)

    /**
     * Fills in the fields using information from the store.
     */
    Status
    setup(const LoginStore &store);

    Status
    setup(const Login &login);
};

Status
ServerRequestJson::setup(const LoginStore &store)
{
    ABC_CHECK(userIdSet(base64Encode(store.userId())));
    if (store.otpKey())
        ABC_CHECK(otpSet(store.otpKey()->totp()));
    return Status();
}

Status
ServerRequestJson::setup(const Login &login)
{
    ABC_CHECK(setup(login.store));
    ABC_CHECK(passwordAuthSet(base64Encode(login.passwordAuth())));
    return Status();
}

Status
loginServerGetGeneral(JsonPtr &result)
{
    const auto url = ABC_SERVER_ROOT "/v1/getinfo";

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    result = replyJson.results();
    return Status();
}

Status
loginServerGetQuestions(JsonPtr &result)
{
    const auto url = ABC_SERVER_ROOT "/v1/questions";

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    result = replyJson.results();
    return Status();
}

Status
loginServerCreate(const LoginStore &store, DataSlice LP1,
                  const CarePackage &carePackage,
                  const LoginPackage &loginPackage,
                  const std::string &syncKey)
{
    const auto url = ABC_SERVER_ROOT "/v1/account/create";
    ServerRequestJson json;
    ABC_CHECK(json.setup(store));
    ABC_CHECK(json.passwordAuthSet(base64Encode(LP1)));
    ABC_CHECK(json.set(ABC_SERVER_JSON_CARE_PACKAGE_FIELD, carePackage.encode()));
    ABC_CHECK(json.set(ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, loginPackage.encode()));
    ABC_CHECK(json.set(ABC_SERVER_JSON_REPO_FIELD, syncKey));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerActivate(const Login &login)
{
    const auto url = ABC_SERVER_ROOT "/v1/account/activate";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerAvailable(const LoginStore &store)
{
    const auto url = ABC_SERVER_ROOT "/v1/account/available";
    ServerRequestJson json;
    ABC_CHECK(json.setup(store));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerAccountUpgrade(const Login &login, JsonPtr rootKeyBox,
                          JsonPtr mnemonicBox, JsonPtr dataKeyBox)
{
    const auto url = ABC_SERVER_ROOT "/v1/account/upgrade";
    struct RequestJson:
        public ServerRequestJson
    {
        ABC_JSON_VALUE(rootKeyBox, "rootKeyBox", JsonPtr)
        ABC_JSON_VALUE(mnemonicBox, "mnemonicBox", JsonPtr)
        ABC_JSON_VALUE(dataKeyBox, "syncDataKeyBox", JsonPtr)
    } json;
    ABC_CHECK(json.setup(login));
    ABC_CHECK(json.rootKeyBoxSet(rootKeyBox));
    ABC_CHECK(json.mnemonicBoxSet(mnemonicBox));
    ABC_CHECK(json.dataKeyBoxSet(dataKeyBox));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerChangePassword(const Login &login,
                          DataSlice newLP1, DataSlice newLRA1,
                          const CarePackage &carePackage,
                          const LoginPackage &loginPackage)
{
    const auto url = ABC_SERVER_ROOT "/v1/account/password/update";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));
    ABC_CHECK(json.set(ABC_SERVER_JSON_NEW_LP1_FIELD, base64Encode(newLP1)));
    ABC_CHECK(json.set(ABC_SERVER_JSON_CARE_PACKAGE_FIELD, carePackage.encode()));
    ABC_CHECK(json.set(ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, loginPackage.encode()));
    if (newLRA1.size())
    {
        ABC_CHECK(json.set(ABC_SERVER_JSON_NEW_LRA1_FIELD, base64Encode(newLRA1)));
    }

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerGetPinPackage(DataSlice DID, DataSlice LPIN1, std::string &result,
                         AuthError &authError)
{
    const auto url = ABC_SERVER_ROOT "/v1/account/pinpackage/get";
    ServerRequestJson json;
    ABC_CHECK(json.set(ABC_SERVER_JSON_DID_FIELD, base64Encode(DID)));
    ABC_CHECK(json.set(ABC_SERVER_JSON_LPIN1_FIELD, base64Encode(LPIN1)));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply, &authError));

    struct ResultJson:
        public JsonObject
    {
        ABC_JSON_CONSTRUCTORS(ResultJson, JsonObject)
        ABC_JSON_STRING(package, "pin_package", nullptr)
    } resultJson(replyJson.results());

    ABC_CHECK(resultJson.packageOk());
    result = resultJson.package();
    return Status();
}

Status
loginServerUpdatePinPackage(const Login &login,
                            DataSlice DID, DataSlice LPIN1,
                            const std::string &pinPackage, time_t ali)
{
    const auto url = ABC_SERVER_ROOT "/v1/account/pinpackage/update";

    // format the ali
    char szALI[DATETIME_LENGTH];
    strftime(szALI, DATETIME_LENGTH, "%Y-%m-%dT%H:%M:%S", gmtime(&ali));

    // Encode those:
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));
    ABC_CHECK(json.set(ABC_SERVER_JSON_DID_FIELD, base64Encode(DID)));
    ABC_CHECK(json.set(ABC_SERVER_JSON_LPIN1_FIELD, base64Encode(LPIN1)));
    ABC_CHECK(json.set(JSON_ACCT_PIN_PACKAGE, pinPackage));
    ABC_CHECK(json.set(ABC_SERVER_JSON_ALI_FIELD, szALI));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerWalletCreate(const Login &login, const std::string &syncKey)
{
    const auto url = ABC_SERVER_ROOT "/v1/wallet/create";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));
    ABC_CHECK(json.set(ABC_SERVER_JSON_REPO_WALLET_FIELD, syncKey));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerWalletActivate(const Login &login, const std::string &syncKey)
{
    const auto url = ABC_SERVER_ROOT "/v1/wallet/activate";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));
    ABC_CHECK(json.set(ABC_SERVER_JSON_REPO_WALLET_FIELD, syncKey));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerOtpEnable(const Login &login, const std::string &otpToken,
                     const long timeout)
{
    const auto url = ABC_SERVER_ROOT "/v1/otp/on";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));
    ABC_CHECK(json.set(ABC_SERVER_JSON_OTP_SECRET_FIELD, otpToken));
    ABC_CHECK(json.set(ABC_SERVER_JSON_OTP_TIMEOUT,
                       static_cast<json_int_t>(timeout)));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerOtpDisable(const Login &login)
{
    const auto url = ABC_SERVER_ROOT "/v1/otp/off";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerOtpStatus(const Login &login, bool &on, long &timeout)
{
    const auto url = ABC_SERVER_ROOT "/v1/otp/status";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    struct ResultJson:
        public JsonObject
    {
        ABC_JSON_CONSTRUCTORS(ResultJson, JsonObject)
        ABC_JSON_BOOLEAN(on, "on", false)
        ABC_JSON_INTEGER(timeout, "otp_timeout", 0)
    } resultJson(replyJson.results());

    on = resultJson.on();
    if (on)
    {
        ABC_CHECK(resultJson.timeoutOk());
        timeout = resultJson.timeout();
    }
    return Status();
}

Status
loginServerOtpReset(const LoginStore &store, const std::string &token)
{
    const auto url = ABC_SERVER_ROOT "/v1/otp/reset";
    struct ResetJson:
        public ServerRequestJson
    {
        ABC_JSON_STRING(otpResetAuth, "otp_reset_auth", nullptr)
    } json;
    ABC_CHECK(json.setup(store));
    ABC_CHECK(json.otpResetAuthSet(token));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerOtpPending(std::list<DataChunk> users, std::list<bool> &isPending)
{
    const auto url = ABC_SERVER_ROOT "/v1/otp/pending/check";

    std::string param;
    std::map<std::string, bool> userMap;
    std::list<std::string> usersEncoded;
    for (const auto &u : users)
    {
        std::string username = base64Encode(u);
        param += (username + ",");
        userMap[username] = false;
        usersEncoded.push_back(username);
    }
    JsonObject json;
    ABC_CHECK(json.set("l1s", param));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    JsonArray arrayJson = replyJson.results();
    size_t size = arrayJson.size();
    for (size_t i = 0; i < size; i++)
    {
        json_t *pJSON_Row = arrayJson[i].get();
        if (!pJSON_Row || !json_is_object(pJSON_Row))
            return ABC_ERROR(ABC_CC_JSONError, "Error parsing JSON array element object");

        json_t *pJSON_Value = json_object_get(pJSON_Row, "login");
        if (!pJSON_Value || !json_is_string(pJSON_Value))
            return ABC_ERROR(ABC_CC_JSONError, "Error otp/pending/login JSON");
        std::string username(json_string_value(pJSON_Value));

        pJSON_Value = json_object_get(pJSON_Row, ABC_SERVER_JSON_OTP_PENDING);
        if (!pJSON_Value || !json_is_boolean(pJSON_Value))
            return ABC_ERROR(ABC_CC_JSONError, "Error otp/pending/pending JSON");
        if (json_is_true(pJSON_Value))
        {
            userMap[username] = json_is_true(pJSON_Value);
        }
    }
    isPending.clear();
    for (auto &username: usersEncoded)
    {
        isPending.push_back(userMap[username]);
    }

    return Status();
}

Status
loginServerOtpResetCancelPending(const Login &login)
{
    const auto url = ABC_SERVER_ROOT "/v1/otp/pending/cancel";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerUploadLogs(const Account *account)
{
    const auto url = ABC_SERVER_ROOT "/v1/account/debug";
    ServerRequestJson json;

    if (account)
    {
        json.setup(account->login); // Failure is fine

        JsonArray jsonArray;
        auto ids = account->wallets.list();
        for (const auto &id: ids)
        {
            DataChunk watchData;
            if (fileLoad(watchData, WalletPaths(id).cachePath()))
            {
                jsonArray.append(
                    json_string(base64Encode(watchData).c_str()));
            }
        }
        json.set("watchers", jsonArray); // Failure is fine
    }

    DataChunk logData = debugLogLoad();
    json.set("log", base64Encode(logData)); // Failure is fine

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));

    return Status();
}

Status
loginServerLogin(LoginJson &result, AuthJson authJson, AuthError *authError)
{
    const auto url = ABC_SERVER_ROOT "/v2/login";

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().get(reply, url, authJson.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply, authError));

    result = replyJson.results();
    return Status();
}

Status
loginServerPasswordSet(AuthJson authJson,
                       DataSlice passwordAuth,
                       JsonPtr passwordKeySnrp,
                       JsonPtr passwordBox,
                       JsonPtr passwordAuthBox)
{
    const auto url = ABC_SERVER_ROOT "/v2/login/password";

    JsonSnrp passwordAuthSnrp;
    ABC_CHECK(passwordAuthSnrp.snrpSet(usernameSnrp()));

    JsonObject dataJson;
    ABC_CHECK(dataJson.set("passwordAuth", base64Encode(passwordAuth)));
    ABC_CHECK(dataJson.set("passwordAuthSnrp", passwordAuthSnrp));
    ABC_CHECK(dataJson.set("passwordKeySnrp", passwordKeySnrp));
    ABC_CHECK(dataJson.set("passwordBox", passwordBox));
    ABC_CHECK(dataJson.set("passwordAuthBox", passwordAuthBox));
    ABC_CHECK(authJson.set("data", dataJson));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().put(reply, url, authJson.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

Status
loginServerRecovery2Set(AuthJson authJson,
                        DataSlice recovery2Id, JsonPtr recovery2Auth,
                        JsonPtr question2Box, JsonPtr recovery2Box,
                        JsonPtr recovery2KeyBox)
{
    const auto url = ABC_SERVER_ROOT "/v2/login/recovery2";

    JsonObject dataJson;
    ABC_CHECK(dataJson.set("recovery2Id", base64Encode(recovery2Id)));
    ABC_CHECK(dataJson.set("recovery2Auth", recovery2Auth));
    ABC_CHECK(dataJson.set("question2Box", question2Box));
    ABC_CHECK(dataJson.set("recovery2Box", recovery2Box));
    ABC_CHECK(dataJson.set("recovery2KeyBox", recovery2KeyBox));
    ABC_CHECK(authJson.set("data", dataJson));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().put(reply, url, authJson.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply));

    return Status();
}

} // namespace abcd
