/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "ABC.h"
#include "HandleCache.hpp"
#include "LoginShim.hpp"
#include "TxDetails.hpp"
#include "TxInfo.hpp"
#include "Version.h"
#include "../abcd/Context.hpp"
#include "../abcd/General.hpp"
#include "../abcd/Export.hpp"
#include "../abcd/account/Account.hpp"
#include "../abcd/account/AccountSettings.hpp"
#include "../abcd/account/AccountCategories.hpp"
#include "../abcd/account/PluginData.hpp"
#include "../abcd/bitcoin/Testnet.hpp"
#include "../abcd/bitcoin/Text.hpp"
#include "../abcd/bitcoin/cache/Cache.hpp"
#include "../abcd/bitcoin/WatcherBridge.hpp"
#include "../abcd/bitcoin/spend/AirbitzFee.hpp"
#include "../abcd/bitcoin/spend/PaymentProto.hpp"
#include "../abcd/bitcoin/spend/Spend.hpp"
#include "../abcd/crypto/Encoding.hpp"
#include "../abcd/crypto/Random.hpp"
#include "../abcd/exchange/ExchangeCache.hpp"
#include "../abcd/http/Http.hpp"
#include "../abcd/http/Uri.hpp"
#include "../abcd/login/Sharing.hpp"
#include "../abcd/login/Bitid.hpp"
#include "../abcd/login/Login.hpp"
#include "../abcd/login/LoginPassword.hpp"
#include "../abcd/login/LoginPin.hpp"
#include "../abcd/login/LoginPin2.hpp"
#include "../abcd/login/LoginRecovery.hpp"
#include "../abcd/login/LoginRecovery2.hpp"
#include "../abcd/login/LoginStore.hpp"
#include "../abcd/login/Otp.hpp"
#include "../abcd/login/RecoveryQuestions.hpp"
#include "../abcd/login/json/LoginPackages.hpp"
#include "../abcd/login/server/LoginServer.hpp"
#include "../abcd/util/Debug.hpp"
#include "../abcd/util/FileIO.hpp"
#include "../abcd/util/Sync.hpp"
#include "../abcd/util/Util.hpp"
#include "../abcd/wallet/Wallet.hpp"
#include <qrencode.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

using namespace abcd;

#define ABC_PROLOG() \
    ABC_DebugLog("%s called", __FUNCTION__); \
    tABC_CC cc = ABC_CC_Ok; \
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok); \
    ABC_CHECK_ASSERT(gContext, ABC_CC_NotInitialized, "The core library has not been initalized")

#define ABC_PROLOG_QUIET() \
    tABC_CC cc = ABC_CC_Ok; \
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok); \
    ABC_CHECK_ASSERT(gContext, ABC_CC_NotInitialized, "The core library has not been initalized")

#define ABC_GET_STORE() \
    std::shared_ptr<LoginStore> store; \
    ABC_CHECK_NEW(cacheLoginStore(store, szUserName))

#define ABC_GET_LOGIN() \
    std::shared_ptr<Login> login; \
    ABC_CHECK_NEW(cacheLogin(login, szUserName))

#define ABC_GET_ACCOUNT() \
    std::shared_ptr<Account> account; \
    ABC_CHECK_NEW(cacheAccount(account, szUserName))

#define ABC_GET_WALLET() \
    std::shared_ptr<Wallet> wallet; \
    ABC_CHECK_NEW(cacheWallet(wallet, szUserName, szWalletUUID));

#define ABC_GET_WALLET_N() \
    std::shared_ptr<Wallet> wallet; \
    ABC_CHECK_NEW(cacheWallet(wallet, nullptr, szWalletUUID));

/** Helper macro for ABC_GetCurrencies. */
#define CURRENCY_GUI_ROW(code, number, name) {#code, number, name, ""},

tABC_CC ABC_Initialize(const char               *szRootDir,
                       const char               *szCaCertPath,
                       const char               *szApiKey,
                       const char               *szAccountType,
                       const char               *szHiddenBitsKey,
                       const unsigned char      *pSeedData,
                       unsigned int             seedLength,
                       tABC_Error               *pError)
{
    // Cannot use ABC_PROLOG - different initialization semantics
    ABC_DebugLog("%s called", __FUNCTION__);
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    ABC_CHECK_ASSERT(!gContext, ABC_CC_Reinitialization,
                     "The core library has already been initalized");
    ABC_CHECK_NULL(szRootDir);
    ABC_CHECK_NULL(szApiKey);
    ABC_CHECK_NULL(szAccountType);
    ABC_CHECK_NULL(szHiddenBitsKey);
    ABC_CHECK_NULL(pSeedData);

    {
        // Initialize the global context object:
        gContext.reset(new Context(szRootDir, szCaCertPath,
                                   szApiKey,
                                   szAccountType,
                                   szHiddenBitsKey));

        // initialize logging
        ABC_CHECK_NEW(debugInitialize());

        ABC_CHECK_NEW(randomInitialize(DataSlice(pSeedData, pSeedData + seedLength)));

        ABC_CHECK_NEW(httpInit());
        ABC_CHECK_NEW(syncInit(szCaCertPath));
    }

exit:
    return cc;
}

/**
 * Mark the end of use of the AirBitz Core library.
 *
 * This function is the counter to ABC_Initialize.
 * It should be called when all use of the library is complete.
 *
 */
void ABC_Terminate()
{
    // Cannot use ABC_PROLOG - no pError
    if (gContext)
    {
        ABC_ClearKeyCache(NULL);
        gContext.reset();

        syncTerminate();

        debugTerminate();
    }
}

void ABC_Log(const char *szMessage)
{
    ABC_DebugLog("%s", szMessage);
}

void ABC_FreeLobby(int hLobby)
{
    gLobbyCache.erase(hLobby);
}

tABC_CC ABC_FetchLobby(char *szId,
                       int *phResult,
                       tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szId);

    {
        auto lobby = std::make_shared<Lobby>();
        ABC_CHECK_NEW(lobbyFetch(*lobby, szId));
        *phResult = gLobbyCache.insert(lobby);
    }

exit:
    return cc;
}

tABC_CC ABC_GetLobbyAccountRequest(int hLobby,
                                   char **pszType,
                                   char **pszDisplayName,
                                   char **pszDisplayImageUrl,
                                   tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszType);
    ABC_CHECK_NULL(pszDisplayName);

    {
        std::shared_ptr<Lobby> lobby;
        ABC_CHECK_NEW(gLobbyCache.find(lobby, hLobby));

        LoginRequest request;
        ABC_CHECK_NEW(loginRequestLoad(request, *lobby));
        *pszType = stringCopy(request.type);
        *pszDisplayName = stringCopy(request.displayName);
        *pszDisplayImageUrl = stringCopy(request.displayImageUrl);
    }

exit:
    return cc;
}

tABC_CC ABC_ApproveLobbyAccountRequest(const char *szUserName,
                                       const char *szPassword,
                                       int hLobby,
                                       tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_LOGIN();
        ABC_GET_ACCOUNT();

        std::shared_ptr<Lobby> lobby;
        ABC_CHECK_NEW(gLobbyCache.find(lobby, hLobby));

        AutoFree<tABC_AccountSettings, accountSettingsFree> settings;
        settings.get() = accountSettingsLoad(*account);

        std::string pin = "";
        if (nullptr != settings->szPIN)
            pin = settings->szPIN;
        ABC_CHECK_NEW(loginRequestApprove(*login, *lobby, pin));
    }

exit:
    return cc;
}

tABC_CC ABC_FixUsername(char **pszResult,
                        const char *szUserName,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszResult);
    ABC_CHECK_NULL(szUserName);

    {
        std::string username;
        ABC_CHECK_NEW(LoginStore::fixUsername(username, szUserName));
        *pszResult = stringCopy(username);
    }

exit:
    return cc;
}

tABC_CC ABC_GetLoginMessages(char **pszJsonResult,
                             tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszJsonResult);

    {
        const auto usernames = gContext->paths.accountList();

        JsonPtr reply;
        ABC_CHECK_NEW(loginServerMessages(reply, usernames));
        *pszJsonResult = stringCopy(reply.encode());
    }

exit:
    return cc;
}

tABC_CC ABC_GetLoginPackages(char **pszResult,
                             const char *szUserName,
                             tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszResult);
    ABC_CHECK_NULL(szUserName);

    {
        ABC_GET_STORE();
        AccountPaths paths;
        ABC_CHECK_NEW(store->paths(paths));

        JsonPtr carePackage, loginPackage;
        ABC_CHECK_NEW(carePackage.load(paths.carePackagePath()));
        ABC_CHECK_NEW(loginPackage.load(paths.loginPackagePath()));

        JsonObject out;
        ABC_CHECK_NEW(out.set("carePackage", carePackage));
        ABC_CHECK_NEW(out.set("loginPackage", loginPackage));
        *pszResult = stringCopy(out.encode());
    }

exit:
    return cc;
}

tABC_CC ABC_PasswordLogin(const char *szUserName,
                          const char *szPassword,
                          char **pszOtpResetToken,
                          char **pszOtpResetDate,
                          tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(pszOtpResetToken);
    ABC_CHECK_NULL(pszOtpResetDate);

    {
        std::shared_ptr<Login> login;
        AuthError authError;
        auto s = cacheLoginPassword(login, szUserName, szPassword,
                                    authError);
        if (!authError.otpToken.empty())
            *pszOtpResetToken = stringCopy(authError.otpToken);
        if (!authError.otpDate.empty())
            *pszOtpResetDate = stringCopy(authError.otpDate);
        ABC_CHECK_NEW(s);
    }

exit:
    return cc;
}

/**
 * Returns success if the requested username is available on the server.
 */
tABC_CC ABC_AccountAvailable(const char *szUserName,
                             tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_STORE();
        ABC_CHECK_NEW(loginServerAvailable(*store));
    }

exit:
    return cc;
}

/**
 * Create a new account.
 *
 * @param szPassword May be null, in which case the account has no password.
 */
tABC_CC ABC_CreateAccount(const char *szUserName,
                          const char *szPassword,
                          tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) >= ABC_MIN_USERNAME_LENGTH, ABC_CC_Error,
                     "Username too short");

    {
        std::string username;
        ABC_CHECK_NEW(LoginStore::fixUsername(username, szUserName));
        if (username.size() < ABC_MIN_USERNAME_LENGTH)
            ABC_RET_ERROR(ABC_CC_NotSupported, "Username is too short");

        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLoginNew(login, szUserName, szPassword));
    }

exit:
    return cc;
}

/**
 * Deletes an account off the local filesystem.
 */
tABC_CC ABC_AccountDelete(const char *szUserName,
                          tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szUserName);

    {
        std::string fixed;
        ABC_CHECK_NEW(LoginStore::fixUsername(fixed, szUserName));
        AccountPaths paths;
        ABC_CHECK_NEW(gContext->paths.accountDir(paths, fixed));

        ABC_CHECK_NEW(fileDelete(paths.dir()));
    }

exit:
    return cc;
}

/**
 * Set the recovery questions for an account
 *
 * This function kicks off a thread to set the recovery questions for an account.
 * The callback will be called when it has finished.
 *
 * @param szUserName                UserName of the account
 * @param szPassword                Password of the account
 * @param szRecoveryQuestions       Recovery questions - newline seperated
 * @param szRecoveryAnswers         Recovery answers - newline seperated
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetAccountRecoveryQuestions(const char *szUserName,
                                        const char *szPassword,
                                        const char *szRecoveryQuestions,
                                        const char *szRecoveryAnswers,
                                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szRecoveryQuestions);
    ABC_CHECK_ASSERT(strlen(szRecoveryQuestions) > 0, ABC_CC_Error,
                     "No recovery questions provided");
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_ASSERT(strlen(szRecoveryAnswers) > 0, ABC_CC_Error,
                     "No recovery answers provided");

    {
        ABC_GET_LOGIN();
        ABC_CHECK_NEW(loginRecoverySet(*login, szRecoveryQuestions,
                                       szRecoveryAnswers));
    }

exit:
    return cc;
}

/**
 * Validates that the provided password is correct.
 * This is used in the GUI to guard access to certain actions.
 */
tABC_CC ABC_PasswordOk(const char *szUserName,
                       const char *szPassword,
                       bool *pOk,
                       tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(pOk);

    {
        ABC_GET_LOGIN();
        ABC_CHECK_NEW(loginPasswordOk(*pOk, *login, szPassword));
    }

exit:
    return cc;
}

/**
 * Returns true if the account has a password configured.
 */
tABC_CC ABC_PasswordExists(const char *szUserName,
                           bool *pExists,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szUserName);

    {
        bool out;
        ABC_CHECK_NEW(loginPasswordExists(out, szUserName));
        *pExists = out;
    }

exit:
    return cc;
}

tABC_CC ABC_Recovery2Key(const char *szUserName,
                         char **pszKey,
                         tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszKey);

    {
        ABC_GET_STORE();
        AccountPaths paths;
        ABC_CHECK_NEW(store->paths(paths));

        DataChunk recovery2Key;
        ABC_CHECK_NEW(loginRecovery2Key(recovery2Key, paths));
        *pszKey = stringCopy(base58Encode(recovery2Key));
    }

exit:
    return cc;
}

tABC_CC ABC_Recovery2Questions(const char *szUserName,
                               const char *szKey,
                               char ***paszQuestions,
                               unsigned int *pCount,
                               tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szKey);
    ABC_CHECK_NULL(paszQuestions);
    ABC_CHECK_NULL(pCount);

    {
        ABC_GET_STORE();
        DataChunk recovery2Key;
        ABC_CHECK_NEW(base58Decode(recovery2Key, szKey));

        std::list<std::string> questions;
        ABC_CHECK_NEW(loginRecovery2Questions(questions, *store, recovery2Key));

        ABC_ARRAY_NEW(*paszQuestions, questions.size(), char *);
        unsigned int i = 0;
        for (const auto &question: questions)
            (*paszQuestions)[i++] = stringCopy(question);
        *pCount = questions.size();
    }

exit:
    return cc;
}

tABC_CC ABC_Recovery2Login(const char *szUserName,
                           const char *szKey,
                           char *aszAnswer1,
                           char *aszAnswer2,
                           char **pszOtpResetToken,
                           char **pszOtpResetDate,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szKey);
    ABC_CHECK_NULL(aszAnswer1);
    ABC_CHECK_NULL(aszAnswer2);
    ABC_CHECK_NULL(pszOtpResetToken);
    ABC_CHECK_NULL(pszOtpResetDate);

    {
        ABC_GET_STORE();
        DataChunk recovery2Key;
        ABC_CHECK_NEW(base58Decode(recovery2Key, szKey));

        std::list<std::string> answers;
        answers.push_back(aszAnswer1);
        answers.push_back(aszAnswer2);

        std::shared_ptr<Login> login;
        AuthError authError;
        auto s = cacheLoginRecovery2(login, szUserName,
                                     recovery2Key, answers, authError);
        if (!authError.otpToken.empty())
            *pszOtpResetToken = stringCopy(authError.otpToken);
        if (!authError.otpDate.empty())
            *pszOtpResetDate = stringCopy(authError.otpDate);
        ABC_CHECK_NEW(s);
    }

exit:
    return cc;
}

tABC_CC ABC_Recovery2Setup(const char *szUserName,
                           const char *szPassword,
                           char *aszQuestion1,
                           char *aszAnswer1,
                           char *aszQuestion2,
                           char *aszAnswer2,
                           char **pszKey,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszKey);
    ABC_CHECK_NULL(aszQuestion1);
    ABC_CHECK_NULL(aszAnswer1);
    ABC_CHECK_NULL(aszQuestion2);
    ABC_CHECK_NULL(aszAnswer2);
    ABC_CHECK_NULL(pszKey);

    {
        ABC_GET_LOGIN();

        std::list<std::string> questions;
        questions.push_back(aszQuestion1);
        questions.push_back(aszQuestion2);

        std::list<std::string> answers;
        answers.push_back(aszAnswer1);
        answers.push_back(aszAnswer2);

        DataChunk recovery2Key;
        ABC_CHECK_NEW(loginRecovery2Set(recovery2Key, *login,
                                        questions, answers));

        *pszKey = stringCopy(base58Encode(recovery2Key));
    }

exit:
    return cc;
}

tABC_CC ABC_Recovery2Delete(const char *szUserName,
                            const char *szPassword,
                            tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_LOGIN();
        ABC_CHECK_NEW(loginRecovery2Delete(*login));
    }

exit:
    return cc;
}

tABC_CC ABC_GetLoginKey(const char *szUserName,
                        const char *szPassword,
                        char **pszKey,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszKey);

    {
        ABC_GET_LOGIN();
        *pszKey = stringCopy(base16Encode(login->dataKey()));
    }

exit:
    return cc;
}

tABC_CC ABC_KeyLogin(const char *szUserName,
                     const char *szKey,
                     tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szKey);

    {
        std::shared_ptr<Login> login;
        DataChunk key;
        ABC_CHECK_NEW(base16Decode(key, szKey));
        ABC_CHECK_NEW(cacheLoginKey(login, szUserName, key));
    }

exit:
    return cc;
}

tABC_CC ABC_OtpKeyGet(const char *szUserName,
                      char **pszKey,
                      tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszKey);

    {
        ABC_GET_STORE();

        const OtpKey *key = store->otpKey();
        ABC_CHECK_ASSERT(key, ABC_CC_NULLPtr, "No OTP key in account.");
        *pszKey = stringCopy(key->encodeBase32());
    }

exit:
    return cc;
}

tABC_CC ABC_OtpKeySet(const char *szUserName,
                      char *szKey,
                      tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szKey);

    {
        ABC_GET_STORE();

        OtpKey key;
        ABC_CHECK_NEW(key.decodeBase32(szKey));
        ABC_CHECK_NEW(store->otpKeySet(key));
    }

exit:
    return cc;
}

tABC_CC ABC_OtpKeyRemove(const char *szUserName,
                         tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_STORE();
        ABC_CHECK_NEW(store->otpKeyRemove());
    }

exit:
    return cc;
}

tABC_CC ABC_OtpAuthGet(const char *szUserName,
                       const char *szPassword,
                       bool *pbEnabled,
                       long *pTimeout,
                       tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pbEnabled);
    ABC_CHECK_NULL(pTimeout);

    {
        ABC_GET_LOGIN();
        ABC_CHECK_NEW(otpAuthGet(*login, *pbEnabled, *pTimeout));
    }

exit:
    return cc;
}

tABC_CC ABC_OtpAuthSet(const char *szUserName,
                       const char *szPassword,
                       long timeout,
                       tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_LOGIN();
        ABC_CHECK_NEW(otpAuthSet(*login, timeout));
    }

exit:
    return cc;
}

tABC_CC ABC_OtpAuthRemove(const char *szUserName,
                          const char *szPassword,
                          tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_LOGIN();
        ABC_CHECK_NEW(otpAuthRemove(*login));
    }

exit:
    return cc;
}

tABC_CC ABC_OtpResetGet(char **pszUsernames,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszUsernames);

    {
        std::list<std::string> result;
        ABC_CHECK_NEW(otpResetGet(result, gContext->paths.accountList()));

        std::string out;
        for (auto i: result)
            out += i + "\n";
        *pszUsernames = stringCopy(out);
    }

exit:
    return cc;
}

tABC_CC ABC_OtpResetSet(const char *szUserName,
                        const char *szToken,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szToken);

    {
        ABC_GET_STORE();
        ABC_CHECK_NEW(otpResetSet(*store, szToken));
    }

exit:
    return cc;
}

tABC_CC ABC_OtpResetRemove(const char *szUserName,
                           const char *szPassword,
                           tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_LOGIN();
        ABC_CHECK_NEW(otpResetRemove(*login));
    }

exit:
    return cc;
}

tABC_CC ABC_BitidParseUri(const char *szUserName,
                          const char *szPassword,
                          const char *szBitidURI,
                          char **pszDomain,
                          char **pszBitidCallbackURI,
                          tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szBitidURI);
    ABC_CHECK_NULL(pszDomain);

    {
        Uri callback;
        ABC_CHECK_NEW(bitidCallback(callback, trimSpace(szBitidURI)));
        *pszBitidCallbackURI = stringCopy(callback.encode());

        callback.pathSet("");
        *pszDomain = stringCopy(callback.encode());
    }

exit:
    return cc;
}

tABC_CC ABC_BitidLogin(const char *szUserName,
                       const char *szPassword,
                       const char *szBitidURI,
                       tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szBitidURI);

    {
        ABC_GET_LOGIN();
        ABC_CHECK_NEW(bitidLogin(login->rootKey(), trimSpace(szBitidURI)));
    }

exit:
    return cc;
}

tABC_CC ABC_BitidLoginMeta(const char *szUserName,
                           const char *szPassword,
                           const char *szBitidURI,
                           const char *szWalletUUID,
                           const char *szBitIDKYCURI,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szBitidURI);
    ABC_CHECK_NULL(szWalletUUID);

    {
        ABC_GET_LOGIN();
        ABC_GET_WALLET();

        std::string kycUri;
        if (szBitIDKYCURI)
            kycUri = szBitIDKYCURI;

        ABC_CHECK_NEW(bitidLogin(login->rootKey(), trimSpace(szBitidURI), 0,
                                 wallet.get(), kycUri));
    }

exit:
    return cc;
}

tABC_CC ABC_BitidSign(const char *szUserName,
                      const char *szPassword,
                      const char *szBitidURI,
                      const char *szMessage,
                      char **pszBitidAddress,
                      char **pszBitidSignature,
                      tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szBitidURI);
    ABC_CHECK_NULL(szMessage);
    ABC_CHECK_NULL(pszBitidAddress);
    ABC_CHECK_NULL(pszBitidSignature);

    {
        ABC_GET_LOGIN();

        Uri callback;
        ABC_CHECK_NEW(bitidCallback(callback, trimSpace(szBitidURI), false));
        const auto signature = bitidSign(login->rootKey(),
                                         szMessage, callback.encode(), 0);

        *pszBitidAddress = stringCopy(signature.address);
        *pszBitidSignature = stringCopy(signature.signature);
    }

exit:
    return cc;
}

/**
 * Create a new wallet.
 *
 * This function kicks off a thread to create a new wallet. The callback will be called when it has finished.
 * The UUID of the new wallet will be provided in the callback pRetData as a (char *).
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param szWalletName              Wallet Name
 * @param currencyNum               ISO 4217 currency number
 * @param pszUuid                   Resulting wallet name. The caller frees this.
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CreateWallet(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletName,
                         int        currencyNum,
                         char       **pszUuid,
                         tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szWalletName);
    ABC_CHECK_ASSERT(strlen(szWalletName) > 0, ABC_CC_Error,
                     "No wallet name provided");
    ABC_CHECK_NULL(pszUuid);

    {
        std::shared_ptr<Wallet> wallet;
        ABC_CHECK_NEW(cacheWalletNew(wallet, szUserName, szWalletName, currencyNum));
        *pszUuid = stringCopy(wallet->id());
    }

exit:
    return cc;
}

tABC_CC ABC_WalletLoad(const char *szUserName,
                       const char *szWalletUUID,
                       tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET();
    }

exit:
    return cc;
}

tABC_CC ABC_WalletRemove(const char *szUserName,
                         const char *szWalletUUID,
                         tABC_Error *pError)
{
    ABC_PROLOG();
    {
        ABC_CHECK_NEW(cacheWalletRemove(szUserName, szWalletUUID));
    }

exit:
    return cc;
}

tABC_CC ABC_WalletName(const char *szUserName,
                       const char *szWalletUUID,
                       char **pszResult,
                       tABC_Error *pError)
{
    ABC_PROLOG_QUIET();
    ABC_CHECK_NULL(pszResult);

    {
        ABC_GET_WALLET();
        *pszResult = stringCopy(wallet->name());
    }

exit:
    return cc;
}


tABC_CC ABC_WalletCurrency(const char *szUserName,
                           const char *szWalletUUID,
                           int *pResult,
                           tABC_Error *pError)
{
    ABC_PROLOG_QUIET();
    ABC_CHECK_NULL(pResult);

    {
        ABC_GET_WALLET();
        *pResult = wallet->currency();
    }

exit:
    return cc;
}

tABC_CC ABC_WalletBalance(const char *szUserName,
                          const char *szWalletUUID,
                          int64_t *pResult,
                          tABC_Error *pError)
{
    ABC_PROLOG_QUIET();
    ABC_CHECK_NULL(pResult);

    {
        ABC_GET_WALLET();
        ABC_CHECK_NEW(wallet->balance(*pResult));
    }

exit:
    return cc;
}

/**
 * Clear cached keys.
 *
 * This function clears any keys that might be cached.
 *
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ClearKeyCache(tABC_Error *pError)
{
    ABC_PROLOG();

    cacheLogout();

exit:
    return cc;
}

tABC_CC ABC_GeneralInfoUpdate(tABC_Error *pError)
{
    ABC_PROLOG();

    ABC_CHECK_NEW(generalUpdate());

exit:
    return cc;
}

/**
 * Create a new wallet.
 *
 * This function provides the array of currencies.
 * The array returned should not be modified and should not be deallocated.
 *
 * @param paCurrencyArray           Pointer in which to store the currency array
 * @param pCount                    Pointer in which to store the count of array entries
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetCurrencies(tABC_Currency **paCurrencyArray,
                          int *pCount,
                          tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(paCurrencyArray);
    ABC_CHECK_NULL(pCount);

    {
        static tABC_Currency aCurrencies[] =
        {
            ABC_CURRENCY_LIST(CURRENCY_GUI_ROW)
        };

        *paCurrencyArray = aCurrencies;
        *pCount = std::end(aCurrencies) - std::begin(aCurrencies);
    }

exit:
    return cc;
}

/**
 * Get the categories for an account.
 *
 * This function gets the categories for an account.
 * An array of allocated strings is allocated so the user is responsible for
 * free'ing all the elements as well as the array itself.
 *
 * @param szUserName            UserName for the account
 * @param paszCategories        Pointer to store results (NULL is stored if no categories)
 * @param pCount                Pointer to store result count (can be 0)
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetCategories(const char *szUserName,
                          const char *szPassword,
                          char ***paszCategories,
                          unsigned int *pCount,
                          tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_ACCOUNT();
        AccountCategories categories;
        ABC_CHECK_NEW(accountCategoriesLoad(categories, *account));

        char **aszCategories;
        ABC_ARRAY_NEW(aszCategories, categories.size(), char *);
        size_t i = 0;
        for (const auto &category: categories)
        {
            aszCategories[i++] = stringCopy(category);
        }

        *paszCategories = aszCategories;
        *pCount = categories.size();
    }

exit:
    return cc;
}

/**
 * Add a category for an account.
 *
 * This function adds a category to an account.
 * No attempt is made to avoid a duplicate entry.
 *
 * @param szUserName            UserName for the account
 * @param szCategory            Category to add
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_AddCategory(const char *szUserName,
                        const char *szPassword,
                        char *szCategory,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szCategory);

    {
        ABC_GET_ACCOUNT();
        ABC_CHECK_NEW(accountCategoriesAdd(*account, szCategory));
    }

exit:
    return cc;
}

/**
 * Remove a category from an account.
 *
 * This function removes a category from an account.
 * If there is more than one category with this name, all categories by this name are removed.
 * If the category does not exist, no error is returned.
 *
 * @param szUserName            UserName for the account
 * @param szCategory            Category to remove
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_RemoveCategory(const char *szUserName,
                           const char *szPassword,
                           char *szCategory,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szCategory);

    {
        ABC_GET_ACCOUNT();
        ABC_CHECK_NEW(accountCategoriesRemove(*account, szCategory));
    }

exit:
    return cc;
}

/**
 * Renames a wallet.
 *
 * This function renames the wallet of a given UUID.
 *
 * @param szNewWalletName       New name for the wallet
 */
tABC_CC ABC_RenameWallet(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletUUID,
                         const char *szNewWalletName,
                         tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET();
        ABC_CHECK_NEW(wallet->nameSet(szNewWalletName));
    }

exit:
    return cc;
}

tABC_CC ABC_WalletArchived(const char *szUserName,
                           const char *szWalletUUID,
                           bool *pResult,
                           tABC_Error *pError)
{
    ABC_PROLOG_QUIET();
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_NULL(pResult);

    {
        ABC_GET_ACCOUNT();
        ABC_CHECK_NEW(account->wallets.archived(*pResult, szWalletUUID));
    }

exit:
    return cc;
}

/**
 * Sets (or unsets) the archive bit on a wallet.
 *
 * @param archived              True if the archive bit should be set
 */
tABC_CC ABC_SetWalletArchived(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              unsigned int archived,
                              tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szWalletUUID);

    {
        ABC_GET_ACCOUNT();
        ABC_CHECK_NEW(account->wallets.archivedSet(szWalletUUID, archived));
    }

exit:
    return cc;
}

tABC_CC ABC_RecoveryLogin(const char *szUserName,
                          const char *szRecoveryAnswers,
                          char **pszOtpResetToken,
                          char **pszOtpResetDate,
                          tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_NULL(pszOtpResetToken);
    ABC_CHECK_NULL(pszOtpResetDate);

    {
        std::shared_ptr<Login> login;
        AuthError authError;
        auto s = cacheLoginRecovery(login, szUserName, szRecoveryAnswers,
                                    authError);
        if (!authError.otpToken.empty())
            *pszOtpResetToken = stringCopy(authError.otpToken);
        if (!authError.otpDate.empty())
            *pszOtpResetDate = stringCopy(authError.otpDate);
        ABC_CHECK_NEW(s);
    }

exit:
    return cc;
}

/**
 * Determines whether or not a PIN-based login pagage exists for the given
 * user.
 */
tABC_CC ABC_PinLoginExists(const char *szUserName,
                           bool *pbExists,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(pbExists);

    {
        ABC_GET_STORE();
        AccountPaths paths;
        ABC_CHECK_NEW(store->paths(paths));

        DataChunk pin2Key;
        bool exists = !!loginPin2Key(pin2Key, paths);
        if (!exists)
            ABC_CHECK_NEW(loginPinExists(exists, szUserName));

        *pbExists = exists;
    }

exit:
    return cc;
}

tABC_CC ABC_PinLogin(const char *szUserName,
                     const char *szPin,
                     int *pWaitSeconds,
                     tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szPin);
    ABC_CHECK_NULL(pWaitSeconds);

    {
        std::shared_ptr<Login> login;
        AuthError authError;
        auto s = cacheLoginPin(login, szUserName, szPin, authError);
        *pWaitSeconds = authError.pinWait;
        ABC_CHECK_NEW(s);
    }

exit:
    return cc;
}

tABC_CC ABC_PinSetup(const char *szUserName,
                     const char *szPassword,
                     const char *szPin,
                     tABC_Error *pError)
{
    ABC_PROLOG();

    // Note: For now, there is no difference between calling this function
    // and manually updating the PIN in the settings.
    // All PIN changes run through the settings.
    // This is hardly ideal, but is necessary for API compatibility.

    {
        ABC_GET_ACCOUNT();

        // Validate the PIN:
        ABC_CHECK_ASSERT(ABC_MIN_PIN_LENGTH <= strlen(szPin), ABC_CC_Error,
                         "Pin is too short");
        char *endstr = nullptr;
        strtol(szPin, &endstr, 10);
        ABC_CHECK_ASSERT('\0' == *endstr, ABC_CC_NonNumericPin,
                         "The pin must be numeric.");

        // Update the settings:
        AutoFree<tABC_AccountSettings, accountSettingsFree> settings;
        settings.get() = accountSettingsLoad(*account);
        ABC_FREE_STR(settings->szPIN);
        settings->szPIN = stringCopy(szPin);
        ABC_CHECK_NEW(accountSettingsSave(*account, settings));
    }

exit:
    return cc;
}

tABC_CC ABC_PinCheck(const char *szUserName,
                     const char *szPassword,
                     const char *szPin,
                     bool *pbResult,
                     tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szPin);
    ABC_CHECK_NULL(pbResult);

    {
        ABC_GET_ACCOUNT();

        AutoFree<tABC_AccountSettings, accountSettingsFree> settings;
        settings.get() = accountSettingsLoad(*account);

        *pbResult = false;
        if (settings->szPIN && !strcmp(settings->szPIN, szPin))
            *pbResult = true;
    }

exit:
    return cc;
}

/**
 * List all the accounts currently present on the device.
 * @param szUserNames a newline-separated list of usernames.
 */
tABC_CC ABC_ListAccounts(char **pszUserNames,
                         tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszUserNames);

    {
        auto list = gContext->paths.accountList();

        std::string out;
        for (const auto &username: list)
            out += username + '\n';

        *pszUserNames = stringCopy(out);
    }

exit:
    return cc;
}

/**
 * Export the private seed used to generate all addresses within a wallet.
 * For now, this uses a simple hex dump of the raw data.
 */
tABC_CC ABC_ExportWalletSeed(const char *szUserName,
                             const char *szPassword,
                             const char *szWalletUUID,
                             char **pszWalletSeed,
                             tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET();
        *pszWalletSeed = stringCopy(base16Encode(wallet->bitcoinKey()));
    }

exit:
    return cc;
}

/**
 * Export the private seed used to generate all addresses within a wallet.
 * For now, this uses a simple hex dump of the raw data.
 */
tABC_CC ABC_ExportWalletXPub(const char *szUserName,
                             const char *szPassword,
                             const char *szWalletUUID,
                             char **pszWalletXPub,
                             tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET();
        *pszWalletXPub = stringCopy(wallet->bitcoinXPub());
    }

exit:
    return cc;
}

/**
 * Gets wallet UUIDs for a specified account.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param paWalletUUID          Pointer to store the allocated array of wallet info structs
 * @param pCount                Pointer to store number of wallets in the array
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetWalletUUIDs(const char *szUserName,
                           const char *szPassword,
                           char ***paWalletUUID,
                           unsigned int *pCount,
                           tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_ACCOUNT();

        auto ids = account->wallets.list();
        ABC_ARRAY_NEW(*paWalletUUID, ids.size(), char *);
        int n = 0;
        for (const auto &id: ids)
        {
            (*paWalletUUID)[n++] = stringCopy(id);
        }
        *pCount = ids.size();
    }

exit:
    return cc;
}

/**
 * Set the wallet order for a specified account.
 *
 * This function sets the order of the wallets for an account to the order in the given
 * array.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param aszUUIDArray          Array of UUID strings
 * @param countUUIDs            Number of UUID's in aszUUIDArray
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetWalletOrder(const char *szUserName,
                           const char *szPassword,
                           const char *szUUIDs,
                           tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_ACCOUNT();

        // Make a mutable copy of the input:
        AutoString temp(stringCopy(szUUIDs));

        // Break apart the text:
        unsigned i = 0;
        char *uuid, *brkt;
        for (uuid = strtok_r(temp, "\n", &brkt);
                uuid;
                uuid = strtok_r(nullptr, "\n", &brkt))
        {
            ABC_CHECK_NEW(account->wallets.reorder(uuid, i++));
        }
    }

exit:
    return cc;
}

/**
 * Get the recovery question choices.
 *
 * This is a blocking function that hits the server for the possible recovery
 * questions.
 *
 * @param pOut                      The returned question choices.
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetQuestionChoices(tABC_QuestionChoices **pOut,
                               tABC_Error *pError)
{
    ABC_PROLOG();

    ABC_CHECK_RET(ABC_GeneralGetQuestionChoices(pOut, pError));

exit:
    return cc;
}

/**
 * Free question choices.
 *
 * This function frees the question choices given
 *
 * @param pQuestionChoices  Pointer to question choices to free.
 */
void ABC_FreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_GeneralFreeQuestionChoices(pQuestionChoices);
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
tABC_CC ABC_GetRecoveryQuestions(const char *szUserName,
                                 char **pszQuestions,
                                 tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszQuestions);

    {
        ABC_GET_STORE();

        std::string questions;
        ABC_CHECK_NEW(loginRecoveryQuestions(questions, *store));
        *pszQuestions = stringCopy(questions);
    }

exit:
    return cc;
}

/**
 * Change account password.
 *
 * This function kicks off a thread to change the password for an account.
 * The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param szNewPassword             New Password for the account
 * @param fRequestCallback          The function that will be called when the password change has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ChangePassword(const char *szUserName,
                           const char *szPassword,
                           const char *szNewPassword,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szNewPassword);
    ABC_CHECK_ASSERT(strlen(szNewPassword) > 0, ABC_CC_Error,
                     "No new password provided");

    {
        ABC_GET_LOGIN();
        ABC_CHECK_NEW(loginPasswordSet(*login, szNewPassword));
    }

exit:
    return cc;
}

/**
 * Converts Satoshi to given currency
 *
 * @param satoshi     Amount in Satoshi
 * @param pCurrency   Pointer to location to store amount converted to currency.
 * @param currencyNum Currency ISO 4217 num
 * @param pError      A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SatoshiToCurrency(const char *szUserName,
                              const char *szPassword,
                              int64_t satoshi,
                              double *pCurrency,
                              int currencyNum,
                              tABC_Error *pError)
{
    ABC_PROLOG_QUIET();

    ABC_CHECK_NEW(gContext->exchangeCache.satoshiToCurrency(*pCurrency, satoshi,
                  static_cast<Currency>(currencyNum)));

exit:
    return cc;
}

/**
 * Converts given currency to Satoshi
 *
 * @param currency    Amount in given currency
 * @param currencyNum Currency ISO 4217 num
 * @param pSatoshi    Pointer to location to store amount converted to Satoshi
 * @param pError      A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CurrencyToSatoshi(const char *szUserName,
                              const char *szPassword,
                              double currency,
                              int currencyNum,
                              int64_t *pSatoshi,
                              tABC_Error *pError)
{
    ABC_PROLOG();

    ABC_CHECK_NEW(gContext->exchangeCache.currencyToSatoshi(*pSatoshi, currency,
                  static_cast<Currency>(currencyNum)));

exit:
    return cc;
}

/**
 * Parses a Bitcoin amount string to an integer.
 * @param the amount to parse, in bitcoins
 * @param the integer value, in satoshis, or ABC_INVALID_AMOUNT
 * if something goes wrong.
 * @param decimalPlaces set to ABC_BITCOIN_DECIMAL_PLACE to convert
 * bitcoin to satoshis.
 */
tABC_CC ABC_ParseAmount(const char *szAmount,
                        uint64_t *pAmountOut,
                        unsigned decimalPlaces)
{
    // Cannot use ABC_PROLOG - no pError
    tABC_CC cc = ABC_CC_Ok;

    if (!bc::decode_base10(*pAmountOut, szAmount, decimalPlaces))
        *pAmountOut = ABC_INVALID_AMOUNT;

    return cc;
}

/**
 * Formats a Bitcoin integer amount as a string, avoiding the rounding
 * problems typical with floating-point math.
 * @param amount the number of satoshis
 * @param pszAmountOut a pointer that will hold the output string, in
 * bitcoins. The caller frees the returned value.
 * @param decimalPlaces set to ABC_BITCOIN_DECIMAL_PLACE to convert
 * satoshis to bitcoins.
 */
tABC_CC ABC_FormatAmount(int64_t amount,
                         char **pszAmountOut,
                         unsigned decimalPlaces,
                         bool bAddSign,
                         tABC_Error *pError)
{
    ABC_PROLOG_QUIET();
    ABC_CHECK_NULL(pszAmountOut);

    {
        std::string out;
        if (amount < 0)
        {
            out = bc::encode_base10(-amount, decimalPlaces);
            if (bAddSign)
                out.insert(0, 1, '-');
        }
        else
        {
            out = bc::encode_base10(amount, decimalPlaces);
        }
        *pszAmountOut = stringCopy(out);
    }

exit:
    return cc;
}

/**
 * Creates a receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param pDetails      Pointer to transaction details
 * @param pszRequestID  Pointer to store allocated ID for this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CreateReceiveRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 char **pszRequestID,
                                 tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET();

        AddressMeta address;
        ABC_CHECK_NEW(wallet->addresses.getNew(address));
        *pszRequestID = stringCopy(address.address);
    }

exit:
    return cc;
}

/**
 * Modifies a previously created receive request.
 * Note: the previous details will be free'ed so if the user is using the previous details for this request
 * they should not assume they will be valid after this call.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pDetails      Pointer to transaction details
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ModifyReceiveRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 const char *szRequestID,
                                 tABC_TxDetails *pDetails,
                                 tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET();

        AddressMeta address;
        ABC_CHECK_NEW(wallet->addresses.get(address, szRequestID));
        address.time = time(nullptr);
        address.requestAmount = pDetails->amountSatoshi;
        address.metadata = pDetails;
        ABC_CHECK_NEW(wallet->addresses.save(address));
    }

exit:
    return cc;
}

/**
 * Finalizes a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_FinalizeReceiveRequest(const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   const char *szRequestID,
                                   tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szRequestID);

    {
        ABC_GET_WALLET();
        ABC_CHECK_NEW(wallet->addresses.recycleSet(szRequestID, false));
    }

exit:
    return cc;
}

/**
 * Cancels a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CancelReceiveRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 const char *szRequestID,
                                 tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szRequestID);

    {
        ABC_GET_WALLET();
        ABC_CHECK_NEW(wallet->addresses.recycleSet(szRequestID, true));
    }

exit:
    return cc;
}

void ABC_SpendFree(void *pSpend)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    delete static_cast<Spend *>(pSpend);
}

tABC_CC ABC_SpendNew(const char *szUserName,
                     const char *szWalletUUID,
                     void **ppResult,
                     tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(ppResult);

    {
        ABC_GET_WALLET();
        *ppResult = new Spend(*wallet);
    }

exit:
    return cc;
}

tABC_CC ABC_SpendAddAddress(void *pSpend,
                            const char *szAddress,
                            uint64_t amount,
                            tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);
    ABC_CHECK_NULL(szAddress);

    {
        auto *spend = static_cast<Spend *>(pSpend);
        ABC_CHECK_NEW(spend->addAddress(szAddress, amount));
    }

exit:
    return cc;
}

tABC_CC ABC_SpendAddPaymentRequest(void *pSpend,
                                   tABC_PaymentRequest *pRequest,
                                   tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);
    ABC_CHECK_NULL(pRequest);

    {
        auto *spend = static_cast<Spend *>(pSpend);
        auto *request = static_cast<PaymentRequest *>(pRequest->pInternal);
        ABC_CHECK_NEW(spend->addPaymentRequest(request));
    }

exit:
    return cc;
}

tABC_CC ABC_SpendAddTransfer(void *pSpend,
                             const char *szWalletUUID,
                             uint64_t amount,
                             tABC_TxDetails *pDetails,
                             tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);
    ABC_CHECK_NULL(pDetails);

    {
        auto *spend = static_cast<Spend *>(pSpend);

        std::shared_ptr<Wallet> target;
        ABC_CHECK_NEW(cacheWallet(target, nullptr, szWalletUUID));
        Metadata metadata(pDetails);
        ABC_CHECK_NEW(spend->addTransfer(*target, amount, metadata));
    }

exit:
    return cc;
}

tABC_CC ABC_SpendSetMetadata(void *pSpend,
                             tABC_TxDetails *pDetails,
                             tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);
    ABC_CHECK_NULL(pDetails);

    {
        auto *spend = static_cast<Spend *>(pSpend);
        ABC_CHECK_NEW(spend->metadataSet(pDetails));
    }

exit:
    return cc;
}

tABC_CC ABC_SpendSetFee(void *pSpend,
                        tABC_SpendFeeLevel feeLevel,
                        uint64_t customFeeSatoshi,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);

    {
        auto *spend = static_cast<Spend *>(pSpend);
        ABC_CHECK_NEW(spend->feeSet(feeLevel, customFeeSatoshi));
    }

exit:
    return cc;
}

tABC_CC ABC_SpendGetFee(void *pSpend,
                        uint64_t *pFee,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);
    ABC_CHECK_NULL(pFee);

    {
        auto *spend = static_cast<Spend *>(pSpend);
        ABC_CHECK_NEW(spend->calculateFees(*pFee));
    }

exit:
    return cc;
}

tABC_CC ABC_SpendGetMax(void *pSpend,
                        uint64_t *pMax,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);
    ABC_CHECK_NULL(pMax);

    {
        auto *spend = static_cast<Spend *>(pSpend);
        ABC_CHECK_NEW(spend->calculateMax(*pMax));
    }

exit:
    return cc;
}

tABC_CC ABC_SpendSignTx(void *pSpend,
                        char **pszRawTx,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);
    ABC_CHECK_NULL(pszRawTx);

    {
        auto *spend = static_cast<Spend *>(pSpend);

        DataChunk rawTx;
        ABC_CHECK_NEW(spend->signTx(rawTx));
        *pszRawTx = stringCopy(base16Encode(rawTx));
    }

exit:
    return cc;
}

tABC_CC ABC_SpendBroadcastTx(void *pSpend,
                             char *szRawTx,
                             tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);
    ABC_CHECK_NULL(szRawTx);

    {
        auto *spend = static_cast<Spend *>(pSpend);

        DataChunk rawTx;
        ABC_CHECK_NEW(base16Decode(rawTx, szRawTx));
        ABC_CHECK_NEW(spend->broadcastTx(rawTx));
    }

exit:
    return cc;
}

tABC_CC ABC_SpendSaveTx(void *pSpend,
                        char *szRawTx,
                        char **pszTxId,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);
    ABC_CHECK_NULL(szRawTx);
    ABC_CHECK_NULL(pszTxId);

    {
        auto *spend = static_cast<Spend *>(pSpend);

        DataChunk rawTx;
        ABC_CHECK_NEW(base16Decode(rawTx, szRawTx));
        std::string txid;
        ABC_CHECK_NEW(spend->saveTx(rawTx, txid));
        *pszTxId = stringCopy(txid);
    }

exit:
    return cc;
}

tABC_CC ABC_SpendApprove(void *pSpend,
                         char **pszTxId,
                         tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pSpend);
    ABC_CHECK_NULL(pszTxId);

    {
        auto *spend = static_cast<Spend *>(pSpend);

        DataChunk rawTx;
        ABC_CHECK_NEW(spend->signTx(rawTx));
        ABC_CHECK_NEW(spend->broadcastTx(rawTx));

        std::string txid;
        ABC_CHECK_NEW(spend->saveTx(rawTx, txid));
        *pszTxId = stringCopy(txid);
    }

exit:
    return cc;
}

tABC_CC ABC_SweepKey(const char *szUserName,
                     const char *szPassword,
                     const char *szWalletUUID,
                     const char *szKey,
                     tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szKey);

    {
        ABC_GET_WALLET();

        ParsedUri uri;
        ABC_CHECK_NEW(parseUri(uri, szKey));
        if (uri.wif.empty())
            ABC_RET_ERROR(ABC_CC_ParseError, "Not a Bitcoin private key");
        ABC_CHECK_NEW(bridgeSweepKey(*wallet, uri.wif, uri.address));
    }

exit:
    return cc;
}

/**
 * Gets the transaction specified
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szID              ID of the transaction
 * @param ppTransaction     Location to store allocated transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetTransaction(const char *szUserName,
                           const char *szPassword,
                           const char *szWalletUUID,
                           const char *szID,
                           tABC_TxInfo **ppTransaction,
                           tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET();

        TxInfo info;
        TxStatus status;
        ABC_CHECK_NEW(wallet->cache.txs.info(info, szID));
        ABC_CHECK_NEW(wallet->cache.txs.status(status, szID));
        *ppTransaction = makeTxInfo(*wallet, info, status);
    }

exit:
    return cc;
}

/**
 * Gets the transactions associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetTransactions(const char *szUserName,
                            const char *szPassword,
                            const char *szWalletUUID,
                            int64_t startTime,
                            int64_t endTime,
                            tABC_TxInfo ***paTransactions,
                            unsigned int *pCount,
                            tABC_Error *pError)
{
    ABC_PROLOG_QUIET();

    {
        ABC_GET_WALLET();
        ABC_CHECK_RET(ABC_TxGetTransactions(*wallet, startTime, endTime, paTransactions,
                                            pCount, pError));
    }

exit:
    return cc;
}

/**
 * Searches the transactions associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szQuery           String to match transactions against
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SearchTransactions(const char *szUserName,
                               const char *szPassword,
                               const char *szWalletUUID,
                               const char *szQuery,
                               tABC_TxInfo ***paTransactions,
                               unsigned int *pCount,
                               tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET();
        ABC_CHECK_RET(ABC_TxSearchTransactions(*wallet, szQuery, paTransactions, pCount,
                                               pError));
    }

exit:
    return cc;
}

/**
 * Frees the given transactions
 *
 * @param pTransaction Pointer to transaction
 */
void ABC_FreeTransaction(tABC_TxInfo *pTransaction)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxFreeTransaction(pTransaction);
}

/**
 * Frees the given array of transactions
 *
 * @param aTransactions Array of transactions
 * @param count         Number of transactions
 */
void ABC_FreeTransactions(tABC_TxInfo **aTransactions,
                          unsigned int count)
{
    // Cannot use ABC_PROLOG - no pError
//    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxFreeTransactions(aTransactions, count);
}

/**
 * Sets the details for a specific transaction.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szID              ID of the transaction
 * @param pDetails          Details for the transaction
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetTransactionDetails(const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletUUID,
                                  const char *szID,
                                  tABC_TxDetails *pDetails,
                                  tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szID);
    ABC_CHECK_NULL(pDetails);

    {
        ABC_GET_WALLET();

        TxInfo info;
        ABC_CHECK_NEW(wallet->cache.txs.info(info, szID));
        auto balance = wallet->addresses.balance(info);

        TxMeta meta;
        ABC_CHECK_NEW(wallet->txs.get(meta, info.ntxid));
        meta.metadata = pDetails;
        meta.internal = true;
        ABC_CHECK_NEW(wallet->txs.save(meta, balance, info.fee));
    }

exit:
    return cc;
}

/**
 * Gets the details for a specific existing transaction.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szID              ID of the transaction
 * @param ppDetails         Location to store allocated details for the transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetTransactionDetails(const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletUUID,
                                  const char *szID,
                                  tABC_TxDetails **ppDetails,
                                  tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szID);
    ABC_CHECK_NULL(ppDetails);

    {
        ABC_GET_WALLET();

        TxInfo info;
        ABC_CHECK_NEW(wallet->cache.txs.info(info, szID));

        TxMeta meta;
        ABC_CHECK_NEW(wallet->txs.get(meta, info.ntxid));
        *ppDetails = meta.metadata.toDetails();
    }

exit:
    return cc;
}

/**
 * Frees the given transaction details
 *
 * @param pDetails Ptr to details to free
 */
void ABC_FreeTxDetails(tABC_TxDetails *pDetails)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxDetailsFree(pDetails);
}

/**
 * Gets password rules results for a given password.
 *
 * This function takes a password and evaluates how long it will take to crack as well as
 * returns an array of rules with information on whether it satisfied each rule.
 *
 * @param szPassword        Paassword to check.
 * @param pSecondsToCrack   Location to store the number of seconds it would take to crack the password
 * @param paRules           Pointer to store the allocated array of password rules
 * @param pCountRules       Pointer to store number of password rules in the array
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CheckPassword(const char *szPassword,
                          double *pSecondsToCrack,
                          tABC_PasswordRule ***paRules,
                          unsigned int *pCountRules,
                          tABC_Error *pError)
{
    double secondsToCrack;
    tABC_PasswordRule **aRules = NULL;
    unsigned int count = 0;
    tABC_PasswordRule *pRuleCount = structAlloc<tABC_PasswordRule>();
    tABC_PasswordRule *pRuleLC = structAlloc<tABC_PasswordRule>();
    tABC_PasswordRule *pRuleUC = structAlloc<tABC_PasswordRule>();
    tABC_PasswordRule *pRuleNum = structAlloc<tABC_PasswordRule>();
    // We don't require a special character, but we still include it in our
    // time to crack calculations
    bool bSpecChar = false;
    size_t L = 0;

    ABC_PROLOG();
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(pSecondsToCrack);
    ABC_CHECK_NULL(paRules);
    ABC_CHECK_NULL(pCountRules);

    // we know there will be 4 rules (lots of magic numbers in this function...sorry)
    ABC_ARRAY_NEW(aRules, 4, tABC_PasswordRule *);

    // must have upper case letter
    pRuleUC->szDescription = stringCopy("Must have at least one upper case letter");
    pRuleUC->bPassed = false;
    aRules[count] = pRuleUC;
    count++;

    // must have lower case letter
    pRuleLC->szDescription = stringCopy("Must have at least one lower case letter");
    pRuleLC->bPassed = false;
    aRules[count] = pRuleLC;
    count++;

    // must have number
    pRuleNum->szDescription = stringCopy("Must have at least one number");
    pRuleNum->bPassed = false;
    aRules[count] = pRuleNum;
    count++;

    // must have 10 characters
    char aPassRDesc[64];
    snprintf(aPassRDesc, sizeof(aPassRDesc), "Must have at least %d characters",
             ABC_MIN_PASS_LENGTH);
    pRuleCount->szDescription = stringCopy(aPassRDesc);
    pRuleCount->bPassed = false;
    aRules[count] = pRuleCount;
    count++;

    // check the length
    if (strlen(szPassword) >= ABC_MIN_PASS_LENGTH)
    {
        pRuleCount->bPassed = true;
    }

    // check the other rules
    for (unsigned i = 0; i < strlen(szPassword); i++)
    {
        char c = szPassword[i];
        if (isdigit(c))
        {
            pRuleNum->bPassed = true;
        }
        else if (isalpha(c))
        {
            if (islower(c))
            {
                pRuleLC->bPassed = true;
            }
            else
            {
                pRuleUC->bPassed = true;
            }
        }
        else
        {
            bSpecChar = true;
        }
    }

    // calculate the time to crack

    /*
     From: http://blog.shay.co/password-entropy/
        A common and easy way to estimate the strength of a password is its entropy.
        The entropy is given by H=LlogBase2(N) where L is the length of the password and N is the size of the alphabet, and it is usually measured in bits.
        The entropy measures the number of bits it would take to represent every password of length L under an alphabet with N different symbols.

        For example, a password of 7 lower-case characters (such as: example, polmnni, etc.) has an entropy of H=7logBase2(26)≈32.9bits.
        A password of 10 alpha-numeric characters (such as: P4ssw0Rd97, K5lb42eQa2) has an entropy of H=10logBase2(62)≈59.54bits.

        Entropy makes it easy to compare password strengths, higher entropy means stronger password (in terms of resistance to brute force attacks).
     */
    // Note: (a) the following calculation of is just based upon one method
    //       (b) the guesses per second is arbitrary
    //       (c) it does not take dictionary attacks into account
    L = strlen(szPassword);
    if (L > 0)
    {
        int N = 0;
        if (pRuleLC->bPassed)
        {
            N += 26; // number of lower-case letters
        }
        if (pRuleUC->bPassed)
        {
            N += 26; // number of upper-case letters
        }
        if (pRuleNum->bPassed)
        {
            N += 10; // number of numeric charcters
        }
        if (bSpecChar)
        {
            N += 35; // number of non-alphanumeric characters on keyboard (iOS)
        }
        const double guessesPerSecond =
            1000000.0; // this can be changed based upon the speed of the computer
        // log2(x) = ln(x)/ln(2) = ln(x)*1.442695041
        double entropy = (double) L * log(N) * 1.442695041;
        double vars = pow(2, entropy);
        secondsToCrack = vars / guessesPerSecond;
    }
    else
    {
        secondsToCrack = 0;
    }

    // store final values
    *pSecondsToCrack = secondsToCrack;
    *paRules = aRules;
    aRules = NULL;
    *pCountRules = count;
    count = 0;

exit:
    ABC_FreePasswordRuleArray(aRules, count);

    return cc;
}

/**
 * Free the password rule array.
 *
 * This function frees the password rule array returned from ABC_CheckPassword.
 *
 * @param aRules   Array of pointers to password rules to be free'd
 * @param nCount   Number of elements in the array
 */
void ABC_FreePasswordRuleArray(tABC_PasswordRule **aRules,
                               unsigned int nCount)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    if ((aRules != NULL) && (nCount > 0))
    {
        for (unsigned i = 0; i < nCount; i++)
        {
            ABC_FREE_STR(aRules[i]->szDescription);
            // note we aren't free'ing the string because it uses heap strings
            ABC_CLEAR_FREE(aRules[i], sizeof(tABC_PasswordRule));
        }
        ABC_CLEAR_FREE(aRules, sizeof(tABC_PasswordRule *) * nCount);
    }
}

tABC_CC ABC_QrEncode(const char *szText,
                     unsigned char **paData,
                     unsigned int *pWidth,
                     tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szText);
    ABC_CHECK_NULL(paData);
    ABC_CHECK_NULL(pWidth);

    {
        AutoFree<QRcode, QRcode_free>
        qr(QRcode_encodeString(szText, 0, QR_ECLEVEL_L, QR_MODE_8, 1));
        ABC_CHECK_ASSERT(qr, ABC_CC_Error, "Unable to create QR code");
        size_t size = qr->width * qr->width;

        unsigned char *aData;
        ABC_ARRAY_NEW(aData, size, unsigned char);
        for (unsigned i = 0; i < size; i++)
        {
            aData[i] = qr->data[i] & 0x1;
        }
        *pWidth = qr->width;
        *paData = aData;
    }

exit:
    return cc;
}

tABC_CC ABC_CreateHbits(char **pszResult,
                        char **pszAddress,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszResult);
    ABC_CHECK_NULL(pszAddress);

    {
        std::string key;
        std::string address;
        ABC_CHECK_NEW(hbitsCreate(key, address));
        *pszResult = stringCopy(key);
        *pszAddress = stringCopy(address);
    }

exit:
    return cc;
}

tABC_CC ABC_AddressUriEncode(const char *szAddress,
                             uint64_t amountSatoshi,
                             const char *szLabel,
                             const char *szMessage,
                             const char *szCategory,
                             const char *szRet,
                             char **pszResult,
                             tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szAddress);
    ABC_CHECK_NULL(pszResult);

    {
        Uri uri;
        uri.schemeSet("bitcoin");
        uri.pathSet(szAddress);
        Uri::QueryMap query;
        if (amountSatoshi)
            query["amount"] = bc::encode_base10(amountSatoshi, 8);
        if (szLabel)
            query["label"] = szLabel;
        if (szMessage)
            query["message"] = szMessage;
        if (szCategory)
            query["category"] = szCategory;
        if (szRet)
            query["ret"] = szRet;
        if (query.size())
            uri.queryEncode(query);

        *pszResult = stringCopy(uri.encode());
    }

exit:
    return cc;

}

tABC_CC ABC_ParseUri(char *szURI,
                     tABC_ParsedUri **ppResult,
                     tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szURI);
    ABC_CHECK_NULL(ppResult);

    {
        ParsedUri uri;
        ABC_CHECK_NEW(parseUri(uri, trimSpace(szURI)));

        tABC_ParsedUri *pResult = structAlloc<tABC_ParsedUri>();
        pResult->szAddress = uri.address.empty() ? nullptr :
                             stringCopy(uri.address);
        pResult->szWif = uri.wif.empty() ? nullptr : stringCopy(uri.wif);
        pResult->szPaymentProto = uri.paymentProto.empty() ? nullptr :
                                  stringCopy(uri.paymentProto);
        pResult->szBitidUri = uri.bitidUri.empty() ? nullptr :
                              stringCopy(uri.bitidUri);
        pResult->amountSatoshi = uri.amountSatoshi;
        pResult->szLabel = uri.label.empty() ? nullptr : stringCopy(uri.label);
        pResult->szMessage = uri.message.empty() ? nullptr :
                             stringCopy(uri.message);
        pResult->szCategory = uri.category.empty() ? nullptr :
                              stringCopy(uri.category);
        pResult->bitidPaymentAddress = uri.bitidPaymentAddress;
        pResult->bitidKYCProvider = uri.bitidKycProvider;
        pResult->bitidKYCRequest = uri.bitidKycRequest;

        pResult->szRet = uri.ret.empty() ? nullptr : stringCopy(uri.ret);
        *ppResult = pResult;
    }

exit:
    return cc;
}

void ABC_FreeParsedUri(tABC_ParsedUri *pUri)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    if (pUri)
    {
        stringFree(pUri->szAddress);
        stringFree(pUri->szWif);
        stringFree(pUri->szPaymentProto);
        stringFree(pUri->szBitidUri);
        stringFree(pUri->szLabel);
        stringFree(pUri->szMessage);
        stringFree(pUri->szCategory);
        stringFree(pUri->szRet);
        free(pUri);
    }
}

tABC_CC ABC_FetchPaymentRequest(char *szRequestUri,
                                tABC_PaymentRequest **ppResult,
                                tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szRequestUri);
    ABC_CHECK_NULL(ppResult);

    {
        std::unique_ptr<PaymentRequest> request(new PaymentRequest());
        ABC_CHECK_NEW(request->fetch(szRequestUri));
        std::string domain;
        const auto sigOk = request->signatureOk(domain, szRequestUri);

        tABC_PaymentRequest *pResult = structAlloc<tABC_PaymentRequest>();
        pResult->bSigned = request->signatureExists() && sigOk;
        pResult->szDomain = stringCopy(domain);
        pResult->amountSatoshi = request->amount();
        pResult->szMemo = request->memoOk() ?
                          stringCopy(request->memo()) : nullptr;
        pResult->szMerchant = stringCopy(request->merchant());
        pResult->pInternal = request.release();
        *ppResult = pResult;
    }

exit:
    return cc;
}

void ABC_FreePaymentRequest(tABC_PaymentRequest *pRequest)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    if (pRequest)
    {
        stringFree(pRequest->szDomain);
        stringFree(pRequest->szMemo);
        stringFree(pRequest->szMerchant);
        delete static_cast<PaymentRequest *>(pRequest->pInternal);
        free(pRequest);
    }
}

tABC_CC ABC_AccountSyncExists(const char *szUserName,
                              bool *pResult,
                              tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pResult);

    {
        std::string fixed;
        AccountPaths paths;

        *pResult = LoginStore::fixUsername(fixed, szUserName) &&
                   gContext->paths.accountDir(paths, fixed) &&
                   fileExists(paths.syncDir());
    }

exit:
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
tABC_CC ABC_LoadAccountSettings(const char *szUserName,
                                const char *szPassword,
                                tABC_AccountSettings **ppSettings,
                                tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_ACCOUNT();
        *ppSettings = accountSettingsLoad(*account);
    }

exit:
    return cc;
}

/**
 * Updates the settings for a specific account
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param szPassword    Password for the account associated with the settings
 * @param pSettings    Pointer to settings
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_UpdateAccountSettings(const char *szUserName,
                                  const char *szPassword,
                                  tABC_AccountSettings *pSettings,
                                  tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_ACCOUNT();
        ABC_CHECK_NEW(accountSettingsSave(*account, pSettings));
    }

exit:
    return cc;
}

/**
 * Frees the given account settings
 *
 * @param pSettings Ptr to setting to free
 */
void ABC_FreeAccountSettings(tABC_AccountSettings *pSettings)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    accountSettingsFree(pSettings);
}

tABC_CC ABC_DataSyncAccount(const char *szUserName,
                            const char *szPassword,
                            bool *pbDirty,
                            bool *pbPasswordChanged,
                            tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pbDirty);
    ABC_CHECK_NULL(pbPasswordChanged);

    {
        ABC_GET_ACCOUNT();

        // Sync the account data:
        bool dirty = false;
        ABC_CHECK_NEW(account->sync(dirty));
        *pbDirty = dirty;

        // Non-critical general information update:
        generalUpdate().log();

        // Has the password changed?
        bool passwordChanged = false;
        auto s = account->login.update();
        switch (s.value())
        {
        case ABC_CC_InvalidOTP:
            ABC_CHECK_NEW(s); // Re-raise the error.
            break;
        case ABC_CC_BadPassword:
            passwordChanged = true;
            break;
        default:
            s.log(); // Failure is fine
        }

        *pbPasswordChanged = passwordChanged;
    }

exit:
    return cc;
}

tABC_CC ABC_DataSyncWallet(const char *szUserName,
                           const char *szPassword,
                           const char *szWalletUUID,
                           bool *pbDirty,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pbDirty);

    {
        ABC_GET_WALLET();

        airbitzFeeAutoSend(*wallet).log();

        bool isArchived = false;
        ABC_CHECK_NEW(wallet->account.wallets.archived(isArchived, szWalletUUID));

        // If wallet has been fully loaded in the past and is now archived, do not launch the
        // watchers.
        if (wallet->cache.addressCheckDoneGet() && isArchived)
            ABC_DebugLog("Skipping ABC_DataSyncWallet for archived and address checked wallet");
        else
        {
            bool dirty = false;
            ABC_CHECK_NEW(wallet->sync(dirty));
            *pbDirty = dirty;
        }
    }

exit:
    return cc;
}

/**
 * Start the watcher for a wallet
 *
 * @param szUserName   UserName for the account
 * @param szPassword   Password for the account
 * @param szWalletUUID The wallet watcher to use
 */
tABC_CC ABC_WatcherStart(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletUUID,
                         tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET();
        ABC_CHECK_NEW(bridgeWatcherStart(*wallet));
    }

exit:
    return cc;
}

/**
 * Runs the watcher update loop. This function will run for an arbitrarily
 * long amount of time as it works to keep the watcher up-to-date with the
 * network. To cause the function to return, call ABC_WatcherStop.
 *
 * @param szWalletUUID The wallet watcher to use
 */
tABC_CC ABC_WatcherLoop(const char *szWalletUUID,
                        tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                        void *pData,
                        tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(fAsyncBitCoinEventCallback);

    {
        ABC_GET_WALLET_N();
        ABC_CHECK_NEW(bridgeWatcherLoop(*wallet,
                                        fAsyncBitCoinEventCallback, pData));
    }

exit:
    return cc;
}

tABC_CC ABC_WatcherConnect(const char *szWalletUUID, tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET_N();

        bool isArchived = false;
        ABC_CHECK_NEW(wallet->account.wallets.archived(isArchived, szWalletUUID));

        // If wallet has been fully loaded in the past and is now archived, do not launch the
        // watchers.
        if (wallet->cache.addressCheckDoneGet() && isArchived)
            ABC_DebugLog("Skipping ABC_WatcherConnect for archived and address checked wallet");
        else
            ABC_CHECK_NEW(bridgeWatcherConnect(*wallet));
    }

exit:
    return cc;
}

/**
 * Watch a single address for a wallet.
 * Pass a nullptr address to cancel the priority poll.
 *
 * @param szUserName   DEPRECATED. Completely unused.
 * @param szPassword   DEPRECATED. Completely unused.
 * @param szWalletUUID The wallet watcher to use
 * @param szAddress    The wallet watcher to use
 */
tABC_CC ABC_PrioritizeAddress(const char *szUserName, const char *szPassword,
                              const char *szWalletUUID, const char *szAddress,
                              tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET();

        std::string address;
        if (szAddress)
            address = szAddress;
        wallet->cache.addresses.prioritize(address);
    }

exit:
    return cc;
}

/**
 * Asks the wallet watcher to disconnect from the network.
 *
 * @param szWalletUUID The wallet watcher to use
 */
tABC_CC ABC_WatcherDisconnect(const char *szWalletUUID, tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET_N();
        ABC_CHECK_NEW(bridgeWatcherDisconnect(*wallet));
    }

exit:
    return cc;
}

/**
 * Stop the watcher loop for a wallet.
 *
 * @param szWalletUUID The wallet watcher to use
 */
tABC_CC ABC_WatcherStop(const char *szWalletUUID, tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET_N();
        ABC_CHECK_NEW(bridgeWatcherStop(*wallet));
    }

exit:
    return cc;
}

/**
 * Delete the watcher for a wallet. This must be called after the loop
 * has completely stopped.
 *
 * @param szWalletUUID The wallet watcher to use
 */
tABC_CC ABC_WatcherDelete(const char *szWalletUUID, tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET_N();
        ABC_CHECK_NEW(bridgeWatcherDelete(*wallet));
    }

exit:
    return cc;
}

/**
 * Deletes the on-disk transaction cache for a wallet.
 */
tABC_CC ABC_WatcherDeleteCache(const char *szWalletUUID, tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_WALLET_N();
        wallet->cache.clear();
    }

exit:
    return cc;
}

/**
 * Lookup the transaction height
 *
 * @param szWalletUUID Used to lookup the watcher with the data
 * @param szTxId The "non-malleable" transaction id
 * @param height Pointer to integer to store the results
 */
tABC_CC ABC_TxHeight(const char *szWalletUUID, const char *szTxid,
                     int *height, tABC_Error *pError)
{
    ABC_PROLOG_QUIET();
    ABC_CHECK_NULL(szTxid);

    {
        ABC_GET_WALLET_N();

        TxStatus status;
        ABC_CHECK_NEW(wallet->cache.txs.status(status, szTxid));
        *height = status.height;
    }

exit:
    return cc;
}

/**
 * Lookup the block chain height
 *
 * @param szWalletUUID Used to lookup the watcher with the data
 * @param height Pointer to integer to store the results
 */
tABC_CC ABC_BlockHeight(const char *szWalletUUID, int *height,
                        tABC_Error *pError)
{
    ABC_PROLOG_QUIET();

    {
        *height = gContext->blockCache.height();
        if (*height == 0)
        {
            cc = ABC_CC_Synchronizing;
        }
    }

exit:
    return cc;
}

tABC_CC ABC_PluginDataList(const char *szUserName,
                           const char *szPassword,
                           char ***paszPlugins,
                           unsigned int *pCount,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(paszPlugins);
    ABC_CHECK_NULL(pCount);

    {
        ABC_GET_ACCOUNT();

        auto plugins = pluginDataList(*account);
        ABC_ARRAY_NEW(*paszPlugins, plugins.size(), char *);
        unsigned int i = 0;
        for (const auto &plugin: plugins)
            (*paszPlugins)[i++] = stringCopy(plugin);
        *pCount = plugins.size();
    }

exit:
    return cc;
}

tABC_CC ABC_PluginDataKeys(const char *szUserName,
                           const char *szPassword,
                           const char *szPlugin,
                           char ***paszKeys,
                           unsigned int *pCount,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szPlugin);
    ABC_CHECK_NULL(paszKeys);
    ABC_CHECK_NULL(pCount);

    {
        ABC_GET_ACCOUNT();

        auto keys = pluginDataKeys(*account, szPlugin);
        ABC_ARRAY_NEW(*paszKeys, keys.size(), char *);
        unsigned int i = 0;
        for (const auto &key: keys)
            (*paszKeys)[i++] = stringCopy(key);
        *pCount = keys.size();
    }

exit:
    return cc;
}

tABC_CC ABC_PluginDataGet(const char *szUserName,
                          const char *szPassword,
                          const char *szPlugin,
                          const char *szKey,
                          char **pszData,
                          tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_ACCOUNT();

        std::string data;
        ABC_CHECK_NEW(pluginDataGet(*account, szPlugin, szKey, data));
        *pszData = stringCopy(data);
    }

exit:
    return cc;
}

tABC_CC ABC_PluginDataSet(const char *szUserName,
                          const char *szPassword,
                          const char *szPlugin,
                          const char *szKey,
                          const char *szData,
                          tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_ACCOUNT();
        ABC_CHECK_NEW(pluginDataSet(*account, szPlugin, szKey, szData));
    }

exit:
    return cc;
}

tABC_CC ABC_PluginDataRemove(const char *szUserName,
                             const char *szPassword,
                             const char *szPlugin,
                             const char *szKey,
                             tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_ACCOUNT();
        ABC_CHECK_NEW(pluginDataRemove(*account, szPlugin, szKey));
    }

exit:
    return cc;
}

tABC_CC ABC_PluginDataClear(const char *szUserName,
                            const char *szPassword,
                            const char *szPlugin,
                            tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_ACCOUNT();
        ABC_CHECK_NEW(pluginDataClear(*account, szPlugin));
    }

exit:
    return cc;
}

/**
 * Request an update to the exchange for a currency
 */
tABC_CC
ABC_RequestExchangeRateUpdate(const char *szUserName,
                              const char *szPassword,
                              int currencyNum,
                              tABC_Error *pError)
{
    ABC_PROLOG();

    {
        ABC_GET_ACCOUNT();

        std::set<Currency> currencies;
        currencies.insert(static_cast<Currency>(currencyNum));

        // Find the user's exchange-rate preference:
        AutoFree<tABC_AccountSettings, accountSettingsFree> settings;
        settings.get() = accountSettingsLoad(*account);
        std::string preference = settings->szExchangeRateSource;

        // Move the user's preference to the front of the list:
        ExchangeSources sources = exchangeSources;
        sources.remove(preference);
        sources.push_front(preference);

        // Do the update:
        ABC_CHECK_NEW(gContext->exchangeCache.update(currencies, sources));
    }

exit:
    return cc;
}

tABC_CC
ABC_IsTestNet(bool *pResult, tABC_Error *pError)
{
    // Cannot use ABC_PROLOG - can be called before initialization
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    *pResult = isTestnet();

    return cc;
}

tABC_CC ABC_Version(char **szVersion, tABC_Error *pError)
{
    // Cannot use ABC_PROLOG - can be called before initialization
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    std::string version = ABC_VERSION;
    version += isTestnet() ? "-testnet" : "-mainnet";

    *szVersion = stringCopy(version);

    return cc;
}

tABC_CC ABC_CsvExport(const char *szUserName, /* DEPRECATED */
                      const char *szPassword, /* DEPRECATED */
                      const char *szWalletUUID,
                      int64_t startTime,
                      int64_t endTime,
                      char **szCsvData,
                      tABC_Error *pError)
{
    tABC_TxInfo **paTransactions = nullptr;
    unsigned int count = 0;
    ABC_PROLOG();

    {
        ABC_GET_WALLET();

        ABC_CHECK_RET(ABC_TxGetTransactions(*wallet, startTime, endTime,
                                            &paTransactions, &count, pError));
        ABC_CHECK_ASSERT(0 != count, ABC_CC_NoTransaction, "No transactions to export");

        std::string currency;
        ABC_CHECK_NEW(currencyCode(currency,
                                   static_cast<Currency>(wallet->currency())));

        ABC_CHECK_RET(ABC_ExportFormatCsv(paTransactions, count, szCsvData, pError,
                                          currency));
    }

exit:
    ABC_FreeTransactions(paTransactions, count);
    return cc;
}

tABC_CC ABC_QBOExport(const char *szUserName, /* DEPRECATED */
                      const char *szPassword, /* DEPRECATED */
                      const char *szWalletUUID,
                      int64_t startTime,
                      int64_t endTime,
                      char **szQBOData,
                      tABC_Error *pError)
{
    tABC_TxInfo **paTransactions = nullptr;
    unsigned int count = 0;
    ABC_PROLOG();

    {
        ABC_GET_WALLET();

        ABC_CHECK_RET(ABC_TxGetTransactions(*wallet, startTime, endTime,
                                            &paTransactions, &count, pError));
        ABC_CHECK_ASSERT(0 != count, ABC_CC_NoTransaction, "No transactions to export");

        std::string currency;
        ABC_CHECK_NEW(currencyCode(currency,
                                   static_cast<Currency>(wallet->currency())));
        std::string out;
        ABC_CHECK_NEW(exportFormatQBO(out, paTransactions, count, currency));
        *szQBOData = stringCopy(out);
    }

exit:
    ABC_FreeTransactions(paTransactions, count);
    return cc;
}

tABC_CC ABC_UploadLogs(const char *szUserName,
                       const char *szPassword,
                       tABC_Error *pError)
{
    ABC_PROLOG();

    {
        // Cannot use ABC_GET_ACCOUNT - account is not required to upload logs
        std::shared_ptr<Account> account;
        cacheAccount(account, szUserName);

        ABC_CHECK_NEW(loginServerUploadLogs(account.get()));
    }

exit:
    return cc;
}
