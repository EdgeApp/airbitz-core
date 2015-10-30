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
#define JSON_ACCT_PIN_PACKAGE                   "pin_package"

#define ABC_SERVER_ROOT                     "https://app.auth.airbitz.co/api/v1"

#define ABC_SERVER_JSON_L1_FIELD            "l1"
#define ABC_SERVER_JSON_LP1_FIELD           "lp1"
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

/**
 * The common format shared by outgoing authentication information.
 */
struct ServerRequestJson:
    public JsonObject
{
    ABC_JSON_STRING(authId, "l1", nullptr)
    ABC_JSON_STRING(authKey, "lp1", nullptr)
    ABC_JSON_STRING(recoveryAuthKey, "lra1", nullptr)
    ABC_JSON_STRING(otp, "otp", nullptr)

    /**
     * Fills in the fields using information from the lobby.
     */
    Status
    setup(const Lobby &lobby);

    Status
    setup(const Login &login);
};

Status
ServerRequestJson::setup(const Lobby &lobby)
{
    ABC_CHECK(authIdSet(base64Encode(lobby.authId()).c_str()));
    if (lobby.otpKey())
        ABC_CHECK(otpSet(lobby.otpKey()->totp().c_str()));
    return Status();
}

Status
ServerRequestJson::setup(const Login &login)
{
    ABC_CHECK(setup(login.lobby));
    ABC_CHECK(authKeySet(base64Encode(login.authKey())));
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

Status
loginServerCreate(const Lobby &lobby, DataSlice LP1,
    const CarePackage &carePackage, const LoginPackage &loginPackage,
    const std::string &syncKey)
{
    const auto url = ABC_SERVER_ROOT "/account/create";
    JsonPtr json(json_pack("{ssssssssss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str(),
        ABC_SERVER_JSON_CARE_PACKAGE_FIELD, carePackage.encode().c_str(),
        ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, loginPackage.encode().c_str(),
        ABC_SERVER_JSON_REPO_FIELD, syncKey.c_str()));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    return Status();
}

Status
loginServerActivate(const Login &login)
{
    const auto url = ABC_SERVER_ROOT "/account/activate";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    return Status();
}

Status
loginServerAvailable(const Lobby &lobby)
{
    const auto url = ABC_SERVER_ROOT "/account/available";
    ServerRequestJson json;
    ABC_CHECK(json.setup(lobby));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    return Status();
}

Status
loginServerAccountUpgrade(const Login &login,
    JsonPtr rootKeyBox, JsonPtr mnemonicBox, JsonPtr dataKeyBox)
{
    const auto url = ABC_SERVER_ROOT "/account/upgrade";
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
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    return Status();
}

Status
loginServerChangePassword(const Login &login,
    DataSlice newLP1, DataSlice newLRA1,
    const CarePackage &carePackage, const LoginPackage &loginPackage)
{
    const auto url = ABC_SERVER_ROOT "/account/password/update";
    JsonPtr json(json_pack("{ss, ss, ss, ss, ss}",
        ABC_SERVER_JSON_L1_FIELD,      base64Encode(login.lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD,     base64Encode(login.authKey()).c_str(),
        ABC_SERVER_JSON_NEW_LP1_FIELD, base64Encode(newLP1).c_str(),
        ABC_SERVER_JSON_CARE_PACKAGE_FIELD,  carePackage.encode().c_str(),
        ABC_SERVER_JSON_LOGIN_PACKAGE_FIELD, loginPackage.encode().c_str()));
    if (newLRA1.size())
    {
        json_object_set_new(json.get(), ABC_SERVER_JSON_NEW_LRA1_FIELD,
            json_string(base64Encode(newLRA1).c_str()));
    }

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    return Status();
}

Status
loginServerGetCarePackage(const Lobby &lobby, CarePackage &result)
{
    const auto url = ABC_SERVER_ROOT "/account/carepackage/get";
    ServerRequestJson json;
    ABC_CHECK(json.setup(lobby));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    struct ResultJson:
        public JsonObject
    {
        ABC_JSON_CONSTRUCTORS(ResultJson, JsonObject)
        ABC_JSON_STRING(package, "care_package", nullptr)
    } resultJson(replyJson.results());

    ABC_CHECK(resultJson.packageOk());
    ABC_CHECK(result.decode(resultJson.package()));
    return Status();
}

Status
loginServerGetLoginPackage(const Lobby &lobby,
    DataSlice LP1, DataSlice LRA1, LoginPackage &result, JsonPtr &rootKeyBox)
{
    const auto url = ABC_SERVER_ROOT "/account/loginpackage/get";
    ServerRequestJson json;
    ABC_CHECK(json.setup(lobby));
    if (LP1.size())
        ABC_CHECK(json.authKeySet(base64Encode(LP1)));
    if (LRA1.size())
        ABC_CHECK(json.recoveryAuthKeySet(base64Encode(LRA1)));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    struct ResultJson:
        public JsonObject
    {
        ABC_JSON_CONSTRUCTORS(ResultJson, JsonObject)
        ABC_JSON_STRING(package, "login_package", nullptr)
        ABC_JSON_VALUE(rootKeyBox, "rootKeyBox", JsonPtr)
    } resultJson(replyJson.results());

    ABC_CHECK(resultJson.packageOk());
    ABC_CHECK(result.decode(resultJson.package()));
    if (json_is_object(resultJson.rootKeyBox().get()))
        rootKeyBox = resultJson.rootKeyBox();
    return Status();
}

Status
loginServerGetPinPackage(DataSlice DID, DataSlice LPIN1, std::string &result)
{
    const auto url = ABC_SERVER_ROOT "/account/pinpackage/get";
    JsonPtr json(json_pack("{ss, ss}",
        ABC_SERVER_JSON_DID_FIELD, base64Encode(DID).c_str(),
        ABC_SERVER_JSON_LPIN1_FIELD, base64Encode(LPIN1).c_str()));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

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
    DataSlice DID, DataSlice LPIN1, const std::string &pinPackage,
    time_t ali)
{
    const auto url = ABC_SERVER_ROOT "/account/pinpackage/update";

    // format the ali
    char szALI[DATETIME_LENGTH];
    strftime(szALI, DATETIME_LENGTH, "%Y-%m-%dT%H:%M:%S", gmtime(&ali));

    // Encode those:
    JsonPtr json(json_pack("{ss, ss, ss, ss, ss, ss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(login.lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(login.authKey()).c_str(),
        ABC_SERVER_JSON_DID_FIELD, base64Encode(DID).c_str(),
        ABC_SERVER_JSON_LPIN1_FIELD, base64Encode(LPIN1).c_str(),
        JSON_ACCT_PIN_PACKAGE, pinPackage.c_str(),
        ABC_SERVER_JSON_ALI_FIELD, szALI));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    return Status();
}

Status
loginServerWalletCreate(const Login &login, const std::string &syncKey)
{
    ABC_CHECK_OLD(ABC_WalletServerRepoPost(login.lobby, login.authKey(),
        syncKey, "wallet/create", &error));
    return Status();
}

Status
loginServerWalletActivate(const Login &login, const std::string &syncKey)
{
    ABC_CHECK_OLD(ABC_WalletServerRepoPost(login.lobby, login.authKey(),
        syncKey, "wallet/activate", &error));
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

    JsonPtr json(json_pack("{ssssss}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(LP1).c_str(),
        ABC_SERVER_JSON_REPO_WALLET_FIELD, szWalletAcctKey.c_str()));

    // send the command
    ABC_CHECK_NEW(AirbitzRequest().post(reply, url, json.encode()));

    ABC_CHECK_NEW(replyJson.decode(reply.body));
    ABC_CHECK_NEW(replyJson.ok());

exit:
    return cc;
}

Status
loginServerOtpEnable(const Login &login, const std::string &otpToken, const long timeout)
{
    const auto url = ABC_SERVER_ROOT "/otp/on";
    JsonPtr json(json_pack("{sssssssi}",
        ABC_SERVER_JSON_L1_FIELD, base64Encode(login.lobby.authId()).c_str(),
        ABC_SERVER_JSON_LP1_FIELD, base64Encode(login.authKey()).c_str(),
        ABC_SERVER_JSON_OTP_SECRET_FIELD, otpToken.c_str(),
        ABC_SERVER_JSON_OTP_TIMEOUT, timeout));

    auto key = login.lobby.otpKey();
    if (key)
        json_object_set_new(json.get(), ABC_SERVER_JSON_OTP_FIELD, json_string(key->totp().c_str()));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    return Status();
}

Status
loginServerOtpDisable(const Login &login)
{
    const auto url = ABC_SERVER_ROOT "/otp/off";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    return Status();
}

Status
loginServerOtpStatus(const Login &login, bool &on, long &timeout)
{
    const auto url = ABC_SERVER_ROOT "/otp/status";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

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
loginServerOtpReset(const Lobby &lobby)
{
    const auto url = ABC_SERVER_ROOT "/otp/reset";
    struct ResetJson:
        public ServerRequestJson
    {
        ABC_JSON_STRING(otpResetAuth, "otp_reset_auth", nullptr)
    } json;
    ABC_CHECK(json.setup(lobby));
    ABC_CHECK(json.otpResetAuthSet(gOtpResetAuth));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    return Status();
}

Status
loginServerOtpPending(std::list<DataChunk> users, std::list<bool> &isPending)
{
    const auto url = ABC_SERVER_ROOT "/otp/pending/check";

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
    JsonPtr json(json_pack("{ss}", "l1s", param.c_str()));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    JsonArray arrayJson = replyJson.results();
    if (arrayJson)
    {
        size_t rows = arrayJson.size();
        for (size_t i = 0; i < rows; i++)
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
    }
    isPending.clear();
    for (auto &username: usersEncoded) {
        isPending.push_back(userMap[username]);
    }

    return Status();
}

Status
loginServerOtpResetCancelPending(const Login &login)
{
    const auto url = ABC_SERVER_ROOT "/otp/pending/cancel";
    ServerRequestJson json;
    ABC_CHECK(json.setup(login));

    HttpReply reply;
    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    ServerReplyJson replyJson;
    ABC_CHECK(replyJson.decode(reply.body));
    ABC_CHECK(replyJson.ok());

    return Status();
}

Status
loginServerUploadLogs(const Account *account)
{
    const auto url = ABC_SERVER_ROOT "/account/debug";
    JsonPtr json;
    HttpReply reply;
    DataChunk logData = debugLogLoad();

    if (account)
    {
        JsonArray jsonArray;
        auto ids = account->wallets.list();
        for (const auto &id: ids)
        {
            std::shared_ptr<Wallet> wallet;
            if (cacheWallet(wallet, nullptr, id.c_str()))
            {
                DataChunk watchData;
                ABC_CHECK(fileLoad(watchData, watcherPath(*wallet)));
                jsonArray.append(
                    json_string(base64Encode(watchData).c_str()));
            }
        }

        json.reset(json_pack("{ss, ss, ss}",
            ABC_SERVER_JSON_L1_FIELD, base64Encode(account->login.lobby.authId()).c_str(),
            ABC_SERVER_JSON_LP1_FIELD, base64Encode(account->login.authKey()).c_str(),
            "log", base64Encode(logData).c_str()));
        if (jsonArray)
            json_object_set(json.get(), "watchers", jsonArray.get());
    }
    else
    {
        json.reset(json_pack("{ss}", "log", base64Encode(logData).c_str()));
    }

    ABC_CHECK(AirbitzRequest().post(reply, url, json.encode()));
    return Status();
}

} // namespace abcd
