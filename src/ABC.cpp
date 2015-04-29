/*
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 */

#include "ABC.h"
#include "LoginShim.hpp"
#include "../abcd/General.hpp"
#include "../abcd/Export.hpp"
#include "../abcd/Wallet.hpp"
#include "../abcd/Tx.hpp"
#include "../abcd/account/Account.hpp"
#include "../abcd/account/AccountSettings.hpp"
#include "../abcd/account/AccountCategories.hpp"
#include "../abcd/account/PluginData.hpp"
#include "../abcd/bitcoin/Testnet.hpp"
#include "../abcd/bitcoin/Text.hpp"
#include "../abcd/bitcoin/WatcherBridge.hpp"
#include "../abcd/crypto/Encoding.hpp"
#include "../abcd/crypto/Random.hpp"
#include "../abcd/exchange/Exchange.hpp"
#include "../abcd/login/Lobby.hpp"
#include "../abcd/login/Login.hpp"
#include "../abcd/login/LoginDir.hpp"
#include "../abcd/login/LoginPassword.hpp"
#include "../abcd/login/LoginPin.hpp"
#include "../abcd/login/LoginRecovery.hpp"
#include "../abcd/login/LoginServer.hpp"
#include "../abcd/login/Otp.hpp"
#include "../abcd/util/Debug.hpp"
#include "../abcd/util/FileIO.hpp"
#include "../abcd/util/Json.hpp"
#include "../abcd/util/Sync.hpp"
#include "../abcd/util/URL.hpp"
#include "../abcd/util/Util.hpp"
#include <qrencode.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <jansson.h>
#include <math.h>

using namespace abcd;

static bool gbInitialized = false;

#define ABC_PROLOG() \
    ABC_DebugLog("%s called", __FUNCTION__); \
    tABC_CC cc = ABC_CC_Ok; \
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok); \
    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized")

/** Helper macro for ABC_GetCurrencies. */
#define CURRENCY_GUI_ROW(code, number, name) {#code, number, name, ""},

/**
 * Initialize the AirBitz Core library.
 *
 * The root directory for all file storage is set in this function.
 *
 * @param szRootDir                     The root directory for all files to be saved
 * @param fAsyncBitCoinEventCallback    The function that should be called when there is an asynchronous
 *                                      BitCoin event
 * @param pData                         Pointer to data to be returned back in callback
 * @param pSeedData                     Pointer to data to seed the random number generator
 * @param seedLength                    Length of the seed data
 * @param pError                        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_Initialize(const char                   *szRootDir,
                       const char                   *szCaCertPath,
                       const unsigned char          *pSeedData,
                       unsigned int                 seedLength,
                       tABC_Error                   *pError)
{
    // Cannot use ABC_PROLOG - different initialization semantics
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    DataSlice Seed(pSeedData, pSeedData + seedLength);

    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "The core library has already been initalized");
    ABC_CHECK_NULL(szRootDir);
    ABC_CHECK_NULL(pSeedData);

    setRootDir(szRootDir);

    // initialize logging
    ABC_CHECK_RET(ABC_DebugInitialize(pError));

    // override the alloc and free of janson so we can have a secure method
    json_set_alloc_funcs(ABC_UtilJanssonSecureMalloc, ABC_UtilJanssonSecureFree);

    // initialize URL system
    ABC_CHECK_RET(ABC_URLInitialize(szCaCertPath, pError));

    // initialize Crypto perf checks to determine hashing power
    ABC_CHECK_RET(ABC_InitializeCrypto(pError));
    ABC_CHECK_RET(ABC_CryptoSetRandomSeed(toU08Buf(Seed), pError));

    // initialize sync
    ABC_CHECK_RET(ABC_SyncInit(szCaCertPath, pError));

    gbInitialized = true;

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
    if (gbInitialized == true)
    {
        ABC_ClearKeyCache(NULL);

        ABC_URLTerminate();

        ABC_SyncTerminate();

        ABC_DebugTerminate();

        gbInitialized = false;
    }
}

/**
 * Create a new account.
 *
 * This function kicks off a thread to signin to an account. The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SignIn(const char *szUserName,
                   const char *szPassword,
                   tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");

    {
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLoginPassword(login, szUserName, szPassword), pError);

        // Take this non-blocking opportunity to update the general info:
        ABC_GeneralUpdateInfo(pError);
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
        std::shared_ptr<Lobby> lobby;
        ABC_CHECK_NEW(cacheLobby(lobby, szUserName), pError);
        ABC_CHECK_RET(ABC_LoginServerAvailable(*lobby, pError));
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
    ABC_CHECK_ASSERT(strlen(szUserName) >= ABC_MIN_USERNAME_LENGTH, ABC_CC_Error, "Username too short");

    {
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLoginNew(login, szUserName, szPassword), pError);

        // Take this non-blocking opportunity to update the general info:
        ABC_CHECK_RET(ABC_GeneralUpdateQuestionChoices(pError));
        ABC_CHECK_RET(ABC_GeneralUpdateInfo(pError));
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
        std::string username;
        ABC_CHECK_NEW(Lobby::fixUsername(username, szUserName), pError);

        std::string directory;
        directory = loginDirFind(username);
        ABC_CHECK_ASSERT(!directory.empty(), ABC_CC_AccountDoesNotExist, "Account not found on disk");
        ABC_CHECK_RET(ABC_FileIODeleteRecursive(directory.c_str(), pError));
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
    ABC_CHECK_ASSERT(strlen(szRecoveryQuestions) > 0, ABC_CC_Error, "No recovery questions provided");
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_ASSERT(strlen(szRecoveryAnswers) > 0, ABC_CC_Error, "No recovery answers provided");

    {
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLogin(login, szUserName), pError);
        ABC_CHECK_RET(ABC_LoginRecoverySet(*login,
            szRecoveryQuestions, szRecoveryAnswers, pError));
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
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLogin(login, szUserName), pError);
        ABC_CHECK_RET(ABC_LoginPasswordOk(*login, szPassword, pOk, pError));
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

    {
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLogin(login, szUserName), pError);

        bool out;
        ABC_CHECK_NEW(passwordExists(out, *login), pError);
        *pExists = out;
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
        std::shared_ptr<Lobby> lobby;
        ABC_CHECK_NEW(cacheLobby(lobby, szUserName), pError);

        const OtpKey *key = lobby->otpKey();
        ABC_CHECK_ASSERT(key, ABC_CC_NULLPtr, "No OTP key in account.");
        ABC_STRDUP(*pszKey, key->encodeBase32().c_str());
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
        std::shared_ptr<Lobby> lobby;
        ABC_CHECK_NEW(cacheLobby(lobby, szUserName), pError);

        OtpKey key;
        ABC_CHECK_NEW(key.decodeBase32(szKey), pError);
        ABC_CHECK_NEW(lobby->otpKeySet(key), pError);
    }

exit:
    return cc;
}

tABC_CC ABC_OtpKeyRemove(const char *szUserName,
                         tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Lobby> lobby;
        ABC_CHECK_NEW(cacheLobby(lobby, szUserName), pError);
        ABC_CHECK_NEW(lobby->otpKeyRemove(), pError);
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
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLogin(login, szUserName), pError);
        ABC_CHECK_NEW(otpAuthGet(*login, *pbEnabled, *pTimeout), pError);
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
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLogin(login, szUserName), pError);
        ABC_CHECK_NEW(otpAuthSet(*login, timeout), pError);
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
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLogin(login, szUserName), pError);
        ABC_CHECK_NEW(otpAuthRemove(*login), pError);
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
        ABC_CHECK_NEW(otpResetGet(result, loginDirList()), pError);

        std::string out;
        for (auto i: result)
            out += i + "\n";
        ABC_STRDUP(*pszUsernames, out.c_str());
    }

exit:
    return cc;
}

tABC_CC ABC_OtpResetDate(char **pszDate,
                         tABC_Error *pError)
{
    ABC_PROLOG();

    ABC_STRDUP(*pszDate, gOtpResetDate.c_str());

exit:
    return cc;
}

tABC_CC ABC_OtpResetSet(const char *szUserName,
                        tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Lobby> lobby;
        ABC_CHECK_NEW(cacheLobby(lobby, szUserName), pError);
        ABC_CHECK_NEW(otpResetSet(*lobby), pError);
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
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLogin(login, szUserName), pError);
        ABC_CHECK_NEW(otpResetRemove(*login), pError);
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
    ABC_CHECK_ASSERT(strlen(szWalletName) > 0, ABC_CC_Error, "No wallet name provided");

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        ABC_CHECK_RET(ABC_WalletCreate(account, szWalletName, currencyNum, pszUuid, pError));
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
    ABC_WalletClearCache();

exit:
    return cc;
}

tABC_CC ABC_GeneralInfoUpdate(tABC_Error *pError)
{
    return ABC_GeneralUpdateInfo(pError);
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
 * Get a PIN number (Deprecated!).
 *
 * This function retrieves the PIN for a given account.
 * The string is allocated and must be free'd by the caller.
 * It is deprecated in favor of just reading the PIN out of the account settings.
 *
 * @param szUserName             UserName for the account
 * @param szPassword             Password for the account
 * @param pszPin                 Pointer where to store allocated PIN string.
 *                               This will be set to NULL if there is no PIN.
 * @param pError                 A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetPIN(const char *szUserName,
                   const char *szPassword,
                   char **pszPin,
                   tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pszPin);

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        AutoFree<tABC_AccountSettings, ABC_AccountSettingsFree> settings;
        ABC_CHECK_RET(ABC_AccountSettingsLoad(*account, &settings.get(), pError));

        *pszPin = nullptr;
        if (settings->szPIN)
            ABC_STRDUP(*pszPin, settings->szPIN);
    }

exit:
    return cc;
}

/**
 * Set PIN number for an account (Deprecated!).
 *
 * This function sets the PIN for a given account.
 * It is deprecated in favor of just setting the PIN in the account settings.
 *
 * @param szUserName            UserName for the account
 * @param szPassword            Password for the account
 * @param szPin                 PIN string
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetPIN(const char *szUserName,
                   const char *szPassword,
                   const char *szPin,
                   tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szPin);
    ABC_CHECK_ASSERT(strlen(szPin) >= ABC_MIN_PIN_LENGTH, ABC_CC_Error, "Pin is too short");

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        AutoFree<tABC_AccountSettings, ABC_AccountSettingsFree> settings;
        ABC_CHECK_RET(ABC_AccountSettingsLoad(*account, &settings.get(), pError));

        ABC_FREE_STR(settings->szPIN);
        ABC_STRDUP(settings->szPIN, szPin);
        ABC_CHECK_RET(ABC_AccountSettingsSave(*account, settings, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        ABC_CHECK_RET(ABC_AccountCategoriesLoad(*account, paszCategories, pCount, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        ABC_CHECK_RET(ABC_AccountCategoriesAdd(*account, szCategory, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        ABC_CHECK_RET(ABC_AccountCategoriesRemove(*account, szCategory, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_WalletSetName(wallet, szNewWalletName, pError));
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

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        ABC_CHECK_NEW(account->wallets.archive(szWalletUUID, archived), pError);
    }

exit:
    return cc;
}

/**
 * Checks the validity of the given account answers.
 *
 * This function sets the attributes on a given wallet to those given.
 * The attributes would have already been set when the wallet was created so this would allow them
 * to be changed.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szRecoveryAnswers     Recovery answers - newline seperated
 * @param pbValid               Pointer to boolean into which to store result
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CheckRecoveryAnswers(const char *szUserName,
                                 const char *szRecoveryAnswers,
                                 bool *pbValid,
                                 tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szRecoveryAnswers);

    {
        std::shared_ptr<Login> login;
        Status s = cacheLoginRecovery(login, szUserName, szRecoveryAnswers);
        if (s)
        {
            *pbValid = true;
        }
        else if (ABC_CC_DecryptFailure == s.value())
        {
            *pbValid = false;
        }
        else
        {
            s.toError(*pError);
            cc = s.value();
            goto exit;
        }
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

    ABC_CHECK_RET(ABC_LoginPinExists(szUserName, pbExists, pError));

exit:
    return cc;
}

/**
 * Deletes the PIN login package from the disk.
 */
tABC_CC ABC_PinLoginDelete(const char *szUserName,
                           tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Lobby> lobby;
        ABC_CHECK_NEW(cacheLobby(lobby, szUserName), pError);
        ABC_CHECK_RET(ABC_LoginPinDelete(*lobby, pError));
    }

exit:
    return cc;
}

/**
 * Performs a PIN-based login for the given user.
 */
tABC_CC ABC_PinLogin(const char *szUserName,
                     const char *szPin,
                     tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szPin);

    {
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLoginPin(login, szUserName, szPin), pError);
    }

exit:
    return cc;
}

/**
 * Sets up the data for a pin-based login, both on disk and on the server.
 */
tABC_CC ABC_PinSetup(const char *szUserName,
                     const char *szPassword,
                     tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Login> login;
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheLogin(login, szUserName), pError);
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        AutoFree<tABC_AccountSettings, ABC_AccountSettingsFree> settings;
        ABC_CHECK_RET(ABC_AccountSettingsLoad(*account, &settings.get(), pError));
        ABC_CHECK_NULL(settings->szPIN);

        time_t expires = time(nullptr);
        expires += 60 * settings->minutesAutoLogout;
        ABC_CHECK_RET(ABC_LoginPinSetup(*login, settings->szPIN, expires, pError));
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
        auto list = loginDirList();

        std::string out;
        for (const auto &username: list)
            out += username + '\n';

        ABC_STRDUP(*pszUserNames, out.c_str());
    }

exit:
    return cc;
}

/**
 * Gets information on the given wallet.
 *
 * This function allocates and fills in a wallet info structure with the information
 * associated with the given wallet UUID
 *
 * @param ppWalletInfo          Pointer to store the pointer of the allocated wallet info struct
 */
tABC_CC ABC_GetWalletInfo(const char *szUserName,
                          const char *szPassword,
                          const char *szWalletUUID,
                          tABC_WalletInfo **ppWalletInfo,
                          tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_WalletGetInfo(wallet, ppWalletInfo, pError));
    }

exit:
    return cc;
}

/**
 * Free the wallet info.
 *
 * This function frees the wallet info struct returned from ABC_GetWalletInfo.
 *
 * @param pWalletInfo   Wallet info to be free'd
 */
void ABC_FreeWalletInfo(tABC_WalletInfo *pWalletInfo)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_WalletFreeInfo(pWalletInfo);
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        U08Buf seedBuf; // Do not free
        ABC_CHECK_RET(ABC_WalletGetBitcoinPrivateSeed(wallet, &seedBuf, pError));
        ABC_STRDUP(*pszWalletSeed, base16Encode(seedBuf).c_str());
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto list = account->wallets.list();

        ABC_ARRAY_NEW(*paWalletUUID, list.size(), char*);
        int n = 0;
        for (const auto &i: list)
        {
            ABC_STRDUP((*paWalletUUID)[n++], i.id.c_str());
        }
        *pCount = list.size();
    }

exit:
    return cc;
}

/**
 * DEPRECATED - Gets wallets for a specified account.
 *
 * This function allocates and fills in an array of wallet info structures with the information
 * associated with the wallets of the given user
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param paWalletInfo          Pointer to store the allocated array of wallet info structs
 * @param pCount                Pointer to store number of wallets in the array
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetWallets(const char *szUserName,
                       const char *szPassword,
                       tABC_WalletInfo ***paWalletInfo,
                       unsigned int *pCount,
                       tABC_Error *pError)
{
    tABC_WalletInfo **aWalletInfo = NULL;

    ABC_PROLOG();
    ABC_CHECK_NULL(paWalletInfo);
    ABC_CHECK_NULL(pCount);

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        // Return an empty list by default:
        *paWalletInfo = nullptr;
        *pCount = 0;

        // Only build a list if we have stuff to put inside:
        auto uuids = account->wallets.list();
        if (0 < uuids.size())
        {
            ABC_ARRAY_NEW(aWalletInfo, uuids.size(), tABC_WalletInfo*);

            unsigned n = 0;
            for (const auto &i: uuids)
            {
                auto wallet = ABC_WalletID(*account, i.id.c_str());
                ABC_CHECK_RET(ABC_WalletGetInfo(wallet, &aWalletInfo[n++], pError));
            }

            *paWalletInfo = aWalletInfo;
            *pCount = uuids.size();
            aWalletInfo = NULL;
        }
    }

exit:
    ABC_FreeWalletInfoArray(aWalletInfo, *pCount);

    return cc;
}

/**
 * DEPRECATED - Free the wallet info array.
 *
 * This function frees the wallet info array returned from ABC_GetWallets.
 *
 * @param aWalletInfo   Wallet info array to be free'd
 * @param nCount        Number of elements in the array
 */
void ABC_FreeWalletInfoArray(tABC_WalletInfo **aWalletInfo,
                             unsigned int nCount)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    if ((aWalletInfo != NULL) && (nCount > 0))
    {
        for (unsigned i = 0; i < nCount; i++)
        {
            ABC_WalletFreeInfo(aWalletInfo[i]);
        }

        ABC_FREE(aWalletInfo);
    }
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        // Make a mutable copy of the input:
        AutoString temp;
        ABC_STRDUP(temp.get(), szUUIDs);

        // Break apart the text:
        unsigned i = 0;
        char *uuid, *brkt;
        for (uuid = strtok_r(temp, "\n", &brkt);
             uuid;
             uuid = strtok_r(nullptr, "\n", &brkt))
        {
            ABC_CHECK_NEW(account->wallets.reorder(uuid, i++), pError);
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
        std::shared_ptr<Lobby> lobby;
        ABC_CHECK_NEW(cacheLobby(lobby, szUserName), pError);
        ABC_CHECK_RET(ABC_LoginGetRQ(*lobby, pszQuestions, pError));
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
    ABC_CHECK_ASSERT(strlen(szNewPassword) > 0, ABC_CC_Error, "No new password provided");

    {
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLogin(login, szUserName), pError);
        ABC_CHECK_RET(ABC_LoginPasswordSet(*login, szNewPassword, pError));
        ABC_WalletClearCache(); // added in 23fab303, but no idea why
    }

exit:
    return cc;
}

/**
 * Change account password using recovery answers.
 *
 * This function kicks off a thread to change the password for an account using the
 * recovery answers as account validation.
 * The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szRecoveryAnswers         Recovery answers (each answer seperated by a newline)
 * @param szNewPassword             New Password for the account
 * @param fRequestCallback          The function that will be called when the password change has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ChangePasswordWithRecoveryAnswers(const char *szUserName,
                                              const char *szRecoveryAnswers,
                                              const char *szNewPassword,
                                              tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_ASSERT(strlen(szRecoveryAnswers) > 0, ABC_CC_Error, "No recovery answers provided");
    ABC_CHECK_NULL(szNewPassword);
    ABC_CHECK_ASSERT(strlen(szNewPassword) > 0, ABC_CC_Error, "No new password provided");

    {
        std::shared_ptr<Login> login;
        ABC_CHECK_NEW(cacheLoginRecovery(login, szUserName, szRecoveryAnswers), pError);
        ABC_CHECK_RET(ABC_LoginPasswordSet(*login, szNewPassword, pError));
        ABC_WalletClearCache(); // added in 23fab303, but no idea why
    }

exit:
    return cc;
}

/**
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param szURI     URI to parse
 * @param ppInfo    Pointer to location to store allocated info struct.
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ParseBitcoinURI(const char *szURI,
                            tABC_BitcoinURIInfo **ppInfo,
                            tABC_Error *pError)
{
    ABC_PROLOG();

    ABC_CHECK_RET(ABC_BridgeParseBitcoinURI(szURI, ppInfo, pError));

exit:

    return cc;
}

/**
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param pInfo Pointer to allocated info struct.
 */
void ABC_FreeURIInfo(tABC_BitcoinURIInfo *pInfo)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_BridgeFreeURIInfo(pInfo);
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
    ABC_PROLOG();

    ABC_CHECK_NEW(exchangeSatoshiToCurrency(*pCurrency, satoshi,
        static_cast<Currency>(currencyNum)), pError);

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

    ABC_CHECK_NEW(exchangeCurrencyToSatoshi(*pSatoshi, currency,
        static_cast<Currency>(currencyNum)), pError);

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

    ABC_CHECK_RET(ABC_BridgeParseAmount(szAmount, pAmountOut, decimalPlaces));

exit:
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
    ABC_PROLOG();

    ABC_CHECK_RET(ABC_BridgeFormatAmount(amount, pszAmountOut, decimalPlaces, bAddSign, pError));

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
                                 tABC_TxDetails *pDetails,
                                 char **pszRequestID,
                                 tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxCreateReceiveRequest(wallet, pDetails, pszRequestID, false, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxModifyReceiveRequest(wallet, szRequestID, pDetails, pError));
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

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxFinalizeReceiveRequest(wallet, szRequestID, pError));
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

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxCancelReceiveRequest(wallet, szRequestID, pError));
    }

exit:
    return cc;
}

/**
 * Generate the QR code for a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pszURI        Pointer to string to store URI -optional
 * @param paData        Pointer to store array of data bytes (0x0 white, 0x1 black)
 * @param pWidth        Pointer to store width of image (image will be square)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GenerateRequestQRCode(const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletUUID,
                                  const char *szRequestID,
                                  char **pszURI,
                                  unsigned char **paData,
                                  unsigned int *pWidth,
                                  tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxGenerateRequestQRCode(wallet, szRequestID, pszURI, paData, pWidth, pError));
    }

exit:
    return cc;
}

/**
 * Initiates a send request.
 *
 * Once the given send has been submitted to the block chain, the given callback will
 * be called and the results data will have a pointer to the request id
 *
 * @param szUserName        UserName for the account associated with this request
 * @param szPassword        Password for the account associated with this request
 * @param szWalletUUID      UUID of the wallet associated with this request
 * @param szDestAddress     Bitcoin address (Base58) to which the funds are to be sent
 * @param pDetails          Pointer to transaction details
 * @param fRequestCallback  The function that will be called when the send request process has finished.
 * @param pData             Pointer to data to be returned back in callback,
 *                          or a `char **pszTxID` if callbacks aren't used.
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_InitiateSendRequest(const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                const char *szDestAddress,
                                tABC_TxDetails *pDetails,
                                char **pszTxId,
                                tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet name provided");
    ABC_CHECK_NULL(pDetails);
    ABC_CHECK_NULL(pszTxId);

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        AutoFree<tABC_TxSendInfo, ABC_TxSendInfoFree> pTxSendInfo;
        ABC_CHECK_RET(ABC_TxSendInfoAlloc(&pTxSendInfo.get(),
                                          wallet,
                                          szDestAddress,
                                          pDetails,
                                          pError));
        ABC_CHECK_RET(ABC_TxSend(pTxSendInfo, pszTxId, pError));
    }

exit:
    return cc;
}

/**
 * Initiates a transfer request.
 *
 * Once the given send has been submitted to the block chain, the given callback will
 * be called and the results data will have a pointer to the request id
 *
 * @param szUserName        UserName for the account associated with this request
 * @param szPassword        Password for the account associated with this request
 * @param pTransfer         Struct container src and dest wallet info
 * @param szDestWalletUUID  UUID of the destination wallet
 * @param pDetails          Pointer to transaction details
 * @param fRequestCallback  The function that will be called when the send request process has finished.
 * @param pData             Pointer to data to be returned back in callback,
 *                          or a `char **pszTxID` if callbacks aren't used.
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_InitiateTransfer(const char *szUserName,
                             const char *szPassword,
                             tABC_TransferDetails *pTransfer,
                             tABC_TxDetails *pDetails,
                             char **pszTxId,
                             tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(pTransfer->szSrcWalletUUID);
    ABC_CHECK_ASSERT(strlen(pTransfer->szSrcWalletUUID) > 0, ABC_CC_Error, "No wallet name provided");
    ABC_CHECK_NULL(pTransfer->szDestWalletUUID);
    ABC_CHECK_ASSERT(strlen(pTransfer->szDestWalletUUID) > 0, ABC_CC_Error, "No destination wallet name provided");
    ABC_CHECK_NULL(pDetails);
    ABC_CHECK_NULL(pszTxId);

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto source = ABC_WalletID(*account, pTransfer->szSrcWalletUUID);
        auto dest = ABC_WalletID(*account, pTransfer->szDestWalletUUID);

        AutoString szRequestId;
        AutoString szRequestAddress;
        ABC_CHECK_RET(ABC_TxCreateReceiveRequest(dest, pDetails, &szRequestId.get(), true, pError));
        ABC_CHECK_RET(ABC_TxGetRequestAddress(dest, szRequestId, &szRequestAddress.get(), pError));

        AutoFree<tABC_TxSendInfo, ABC_TxSendInfoFree> pTxSendInfo;
        ABC_CHECK_RET(ABC_TxSendInfoAlloc(&pTxSendInfo.get(),
                                          source,
                                          szRequestAddress,
                                          pDetails,
                                          pError));

        pTxSendInfo->bTransfer = true;
        ABC_CHECK_RET(ABC_WalletIDCopy(&pTxSendInfo->walletDest, dest, pError));
        ABC_STRDUP(pTxSendInfo->szDestName, pTransfer->szDestName);
        ABC_STRDUP(pTxSendInfo->szDestCategory, pTransfer->szDestCategory);
        ABC_STRDUP(pTxSendInfo->szSrcName, pTransfer->szSrcName);
        ABC_STRDUP(pTxSendInfo->szSrcCategory, pTransfer->szSrcCategory);
        ABC_CHECK_RET(ABC_TxSend(pTxSendInfo, pszTxId, pError));
    }

exit:
    return cc;
}

tABC_CC ABC_CalcSendFees(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletUUID,
                         const char *szDestAddress,
                         bool bTransfer,
                         tABC_TxDetails *pDetails,
                         int64_t *pTotalFees,
                         tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet name provided");
    ABC_CHECK_NULL(pDetails);

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        AutoFree<tABC_TxSendInfo, ABC_TxSendInfoFree> pTxSendInfo;
        ABC_CHECK_RET(ABC_TxSendInfoAlloc(&pTxSendInfo.get(),
                                          wallet,
                                          szDestAddress,
                                          pDetails,
                                          pError));
        pTxSendInfo->bTransfer = bTransfer;
        if (bTransfer)
        {
            auto dest = ABC_WalletID(*account, pTxSendInfo->szDestAddress);

            AutoString szRequestId;
            AutoString szRequestAddress;
            ABC_CHECK_RET(ABC_TxCreateReceiveRequest(dest, pDetails, &szRequestId.get(), true, pError));
            ABC_CHECK_RET(ABC_TxGetRequestAddress(dest, szRequestId, &szRequestAddress.get(), pError));
            ABC_FREE_STR(pTxSendInfo->szDestAddress);
            ABC_STRDUP(pTxSendInfo->szDestAddress, szRequestAddress);
        }

        ABC_CHECK_RET(ABC_TxDupDetails(&(pTxSendInfo->pDetails), pDetails, pError));
        ABC_CHECK_RET(ABC_TxCalcSendFees(pTxSendInfo, pTotalFees, pError));

        pDetails->amountFeesAirbitzSatoshi = pTxSendInfo->pDetails->amountFeesAirbitzSatoshi;
        pDetails->amountFeesMinersSatoshi = pTxSendInfo->pDetails->amountFeesMinersSatoshi;
    }

exit:
    return cc;
}

tABC_CC ABC_MaxSpendable(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletUUID,
                         const char *szDestAddress,
                         bool bTransfer,
                         uint64_t *pMaxSatoshi,
                         tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_BridgeMaxSpendable(wallet, szDestAddress, bTransfer, pMaxSatoshi, pError));
    }

exit:
    return cc;
}

/**
 * Sweeps a private key into the wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szKey             Private key in WIF format
 * @param pszAddress        Resulting bitcoin address out
 * @param fCallback         Called when the sweep is done.
 * @param pData             Closure parameter for the callback.
 */
tABC_CC ABC_SweepKey(const char *szUserName,
                     const char *szPassword,
                     const char *szWalletUUID,
                     const char *szKey,
                     char **pszAddress,
                     tABC_Sweep_Done_Callback fCallback,
                     void *pData,
                     tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        AutoU08Buf key;
        bool bCompressed;
        ABC_CHECK_RET(ABC_BridgeDecodeWIF(szKey, &key, &bCompressed, pszAddress, pError));

        ABC_CHECK_RET(ABC_BridgeSweepKey(wallet, key, bCompressed, fCallback, pData, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxGetTransaction(wallet, szID, ppTransaction, pError));
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
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxGetTransactions(wallet, startTime, endTime, paTransactions, pCount, pError));
        ABC_CHECK_RET(ABC_BridgeFilterTransactions(wallet, *paTransactions, pCount, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxSearchTransactions(wallet, szQuery, paTransactions, pCount, pError));
    }

exit:
    return cc;
}

/**
 * Obtains a raw Bitcoin transaction in hex form.
 */
tABC_CC ABC_GetRawTransaction(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              const char *szID,
                              char **pszHex,
                              tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        AutoFree<tABC_TxInfo, ABC_FreeTransaction> info;
        ABC_CHECK_RET(ABC_TxGetTransaction(wallet, szID, &info.get(), pError));

        DataChunk tx;
        ABC_CHECK_NEW(watcherBridgeRawTx(wallet, info->szMalleableTxId, tx), pError);
        ABC_STRDUP(*pszHex, base16Encode(tx).c_str());
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
    ABC_DebugLog("%s called", __FUNCTION__);

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

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxSetTransactionDetails(wallet, szID, pDetails, pError));
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

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxGetTransactionDetails(wallet, szID, ppDetails, pError));
    }

exit:
    return cc;
}

/**
 * Gets the bit coin public address for a specified request
 *
 * @param szUserName        UserName for the account associated with the requests
 * @param szPassword        Password for the account associated with the requests
 * @param szWalletUUID      UUID of the wallet associated with the requests
 * @param szRequestID       ID of request
 * @param pszAddress        Location to store allocated address string (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetRequestAddress(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              const char *szRequestID,
                              char **pszAddress,
                              tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxGetRequestAddress(wallet, szRequestID, pszAddress, pError));
    }

exit:
    return cc;
}

/**
 * Gets the pending requests associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the requests
 * @param szPassword        Password for the account associated with the requests
 * @param szWalletUUID      UUID of the wallet associated with the requests
 * @param paTransactions    Pointer to store array of requests info pointers
 * @param pCount            Pointer to store number of requests
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetPendingRequests(const char *szUserName,
                               const char *szPassword,
                               const char *szWalletUUID,
                               tABC_RequestInfo ***paRequests,
                               unsigned int *pCount,
                               tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxGetPendingRequests(wallet, paRequests, pCount, pError));
    }

exit:
    return cc;
}

/**
 * Frees the given array of requets
 *
 * @param aRequests Array of requests
 * @param count     Number of requests
 */
void ABC_FreeRequests(tABC_RequestInfo **aRequests,
                      unsigned int count)
{
    // Cannot use ABC_PROLOG - no pError
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxFreeRequests(aRequests, count);
}

/**
 * Duplicates transaction details.
 * This can be used when changing the details on a transaction.
 *
 * @param ppNewDetails  Address to store pointer to copy of details
 * @param pOldDetails   Ptr to details to copy
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_DuplicateTxDetails(tABC_TxDetails **ppNewDetails,
                               const tABC_TxDetails *pOldDetails,
                               tABC_Error *pError)
{
    ABC_PROLOG();

    ABC_CHECK_RET(ABC_TxDupDetails(ppNewDetails, pOldDetails, pError));

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

    ABC_TxFreeDetails(pDetails);
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
    tABC_PasswordRule *pRuleCount = NULL;
    tABC_PasswordRule *pRuleLC = NULL;
    tABC_PasswordRule *pRuleUC = NULL;
    tABC_PasswordRule *pRuleNum = NULL;
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
    ABC_ARRAY_NEW(aRules, 4, tABC_PasswordRule*);

    // must have upper case letter
    ABC_NEW(pRuleUC, tABC_PasswordRule);
    ABC_STRDUP(pRuleUC->szDescription, "Must have at least one upper case letter");
    pRuleUC->bPassed = false;
    aRules[count] = pRuleUC;
    count++;

    // must have lower case letter
    ABC_NEW(pRuleLC, tABC_PasswordRule);
    ABC_STRDUP(pRuleLC->szDescription, "Must have at least one lower case letter");
    pRuleLC->bPassed = false;
    aRules[count] = pRuleLC;
    count++;

    // must have number
    ABC_NEW(pRuleNum, tABC_PasswordRule);
    ABC_STRDUP(pRuleNum->szDescription, "Must have at least one number");
    pRuleNum->bPassed = false;
    aRules[count] = pRuleNum;
    count++;

    // must have 10 characters
    ABC_NEW(pRuleCount, tABC_PasswordRule);
    char aPassRDesc[64];
    snprintf(aPassRDesc, sizeof(aPassRDesc), "Must have at least %d characters", ABC_MIN_PASS_LENGTH);
    ABC_STRDUP(pRuleCount->szDescription, aPassRDesc);
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
        const double guessesPerSecond = 1000000.0; // this can be changed based upon the speed of the computer
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
        AutoFree<QRcode, QRcode_free> qr =
            QRcode_encodeString(szText, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        ABC_CHECK_RET(ABC_AccountSettingsLoad(*account, ppSettings, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        ABC_CHECK_RET(ABC_AccountSettingsSave(*account, pSettings, pError));
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

    ABC_AccountSettingsFree(pSettings);
}

tABC_CC ABC_DataSyncAccount(const char *szUserName,
                            const char *szPassword,
                            tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                            void *pData,
                            tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        // Sync the account data:
        bool dirty = false;
        ABC_CHECK_NEW(account->sync(dirty), pError);
        if (dirty && fAsyncBitCoinEventCallback)
        {
            // Try to clear the wallet cache in case the Wallets list changed
            ABC_WalletClearCache();

            tABC_AsyncBitCoinInfo info;
            info.eventType = ABC_AsyncEventType_DataSyncUpdate;
            info.pData = pData;
            ABC_STRDUP(info.szDescription, "Account Updated");
            fAsyncBitCoinEventCallback(&info);
            ABC_FREE_STR(info.szDescription);
        }

        // Get the server keys:
        AutoU08Buf LP1;
        ABC_CHECK_RET(ABC_LoginGetServerKey(account->login(), &LP1, pError));

        // Has the password changed?
        tABC_Error error;
        LoginPackage loginPackage;
        cc = ABC_LoginServerGetLoginPackage(account->login().lobby(), LP1, U08Buf(), loginPackage, &error);

        if (cc == ABC_CC_InvalidOTP)
        {
            ABC_CHECK_RET(cc);
        }
        else if (cc == ABC_CC_BadPassword)
        {
            if (fAsyncBitCoinEventCallback)
            {
                tABC_AsyncBitCoinInfo info;
                info.eventType = ABC_AsyncEventType_RemotePasswordChange;
                info.pData = pData;
                ABC_STRDUP(info.szDescription, "Password changed");
                fAsyncBitCoinEventCallback(&info);
                ABC_FREE_STR(info.szDescription);
            }
            goto exit;
        }
    }

exit:
    return cc;
}

tABC_CC ABC_DataSyncWallet(const char *szUserName,
                           const char *szPassword,
                           const char *szWalletUUID,
                           tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                           void *pData,
                           tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(fAsyncBitCoinEventCallback);

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        bool dirty = false;
        ABC_CHECK_RET(ABC_WalletSyncData(wallet, dirty, pError));
        if (dirty)
        {
            tABC_AsyncBitCoinInfo info;
            info.eventType = ABC_AsyncEventType_DataSyncUpdate;
            info.pData = pData;
            ABC_STRDUP(info.szDescription, "Wallet Updated");
            fAsyncBitCoinEventCallback(&info);
            ABC_FREE_STR(info.szDescription);
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_BridgeWatcherStart(wallet, pError));
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

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, nullptr), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_BridgeWatcherLoop(wallet, fAsyncBitCoinEventCallback, pData, pError));
    }

exit:
    return cc;
}

tABC_CC ABC_WatcherConnect(const char *szWalletUUID, tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, nullptr), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_BridgeWatcherConnect(wallet, pError));
    }

exit:
    return cc;
}

/**
 * Watch all the addresses for szWalletUUID
 *
 * @param szUserName   UserName for the account
 * @param szPassword   Password for the account
 * @param szWalletUUID The wallet watcher to use
 */
tABC_CC ABC_WatchAddresses(const char *szUserName, const char *szPassword,
                           const char *szWalletUUID, tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_TxWatchAddresses(wallet, pError));
    }

exit:
    return cc;
}

/**
 * Watch a single address for a wallet
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_BridgePrioritizeAddress(wallet, szAddress, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, nullptr), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_BridgeWatcherDisconnect(wallet, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, nullptr), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_BridgeWatcherStop(wallet, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, nullptr), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_BridgeWatcherDelete(wallet, pError));
    }

exit:
    return cc;
}

/**
 * Lookup the transaction height
 *
 * @param szWalletUUID Used to lookup the watcher with the data
 * @param szTxId The "malleable" transaction id
 * @param height Pointer to integer to store the results
 */
tABC_CC ABC_TxHeight(const char *szWalletUUID, const char *szTxId,
                     unsigned int *height, tABC_Error *pError)
{
    ABC_PROLOG();
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet uuid provided");
    ABC_CHECK_NULL(szTxId);
    ABC_CHECK_ASSERT(strlen(szTxId) > 0, ABC_CC_Error, "No tx id provided");

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, nullptr), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_BridgeTxHeight(wallet, szTxId, height, pError));
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
tABC_CC ABC_BlockHeight(const char *szWalletUUID, unsigned int *height, tABC_Error *pError)
{
    ABC_PROLOG();

    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet uuid provided");

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, nullptr), pError);
        auto wallet = ABC_WalletID(*account, szWalletUUID);

        ABC_CHECK_RET(ABC_BridgeTxBlockHeight(wallet, height, pError));
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        std::string data;
        ABC_CHECK_NEW(pluginDataGet(*account, szPlugin, szKey, data), pError);
        ABC_STRDUP(*pszData, data.c_str());
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        ABC_CHECK_NEW(pluginDataSet(*account, szPlugin, szKey, szData), pError);
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        ABC_CHECK_NEW(pluginDataRemove(*account, szPlugin, szKey), pError);
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        ABC_CHECK_NEW(pluginDataClear(*account, szPlugin), pError);
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
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        std::set<Currency> currencies;
        currencies.insert(static_cast<Currency>(currencyNum));

        // Find the user's exchange-rate preference:
        AutoFree<tABC_AccountSettings, ABC_AccountSettingsFree> settings;
        ABC_CHECK_RET(ABC_AccountSettingsLoad(*account, &settings.get(), pError));
        std::string preference = settings->szExchangeRateSource;

        // Move the user's preference to the front of the list:
        ExchangeSources sources = exchangeSources;
        sources.remove(preference);
        sources.push_front(preference);

        // Do the update:
        ABC_CHECK_NEW(exchangeUpdate(currencies, sources), pError);
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

    ABC_STRDUP(*szVersion, version.c_str());

exit:
    return cc;
}

tABC_CC ABC_CsvExport(const char *szUserName, /* DEPRECATED */
                      const char *szPassword, /* DEPRECATED */
                      const char *szUUID,
                      int64_t startTime,
                      int64_t endTime,
                      char **szCsvData,
                      tABC_Error *pError)
{
    tABC_TxInfo **pTransactions = nullptr;
    unsigned int iCount = 0;
    ABC_PROLOG();

    ABC_CHECK_RET(ABC_GetTransactions(szUserName,
                                      szPassword,
                                      szUUID,
                                      startTime, endTime,
                                      &pTransactions, &iCount, pError));
    ABC_CHECK_ASSERT(iCount > 0,
        ABC_CC_NoTransaction, "Unable to find any transactions with that date range");
    ABC_CHECK_RET(ABC_ExportFormatCsv(pTransactions, iCount, szCsvData, pError));

exit:
    ABC_FreeTransactions(pTransactions, iCount);
    return cc;
}

tABC_CC ABC_UploadLogs(const char *szUserName,
                       const char *szPassword,
                       tABC_Error *pError)
{
    ABC_PROLOG();

    {
        std::shared_ptr<Account> account;
        ABC_CHECK_NEW(cacheAccount(account, szUserName), pError);

        ABC_CHECK_RET(ABC_LoginServerUploadLogs(*account, pError));
    }

exit:
    return cc;
}
