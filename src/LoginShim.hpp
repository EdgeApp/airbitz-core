/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * AirBitz Login functions.
 *
 * This file wrapps the methods of `LoginObject.hpp` with a caching layer
 * for backwards-compatibility with the old API.
 */

#ifndef ABC_LoginShim_h
#define ABC_LoginShim_h

#include "HandleCache.hpp"
#include "../abcd/json/JsonPtr.hpp"
#include "../abcd/util/Data.hpp"
#include "../abcd/util/Status.hpp"
#include <memory>

namespace abcd {

class Account;
class Login;
class LoginStore;
class Wallet;
struct AuthError;
struct Lobby;

extern HandleCache<Lobby> gLobbyCache;

/**
 * Clears all cached login objects.
 */
void
cacheLogout();

/**
 * Loads the store for the given user into the cache.
 * If the username is null, the function returns whatever is cached.
 */
Status
cacheLoginStore(std::shared_ptr<LoginStore> &result, const char *szUserName);

/**
 * Creates a new account and adds it to the cache.
 */
Status
cacheLoginNew(std::shared_ptr<Login> &result,
              const char *szUserName, const char *szPassword);

/**
 * Logs the user in with a password, if necessary.
 */
Status
cacheLoginPassword(std::shared_ptr<Login> &result,
                   const char *szUserName, const std::string &password,
                   AuthError &authError);

/**
 * Logs the user in with their recovery answers, if necessary.
 */
Status
cacheLoginRecovery(std::shared_ptr<Login> &result,
                   const char *szUserName, const std::string &recoveryAnswers,
                   AuthError &authError);

/**
 * Logs the user in with their v2 recovery answers, if necessary.
 */
Status
cacheLoginRecovery2(std::shared_ptr<Login> &result,
                    const char *szUserName, DataSlice recovery2Key,
                    const std::list<std::string> &answers,
                    AuthError &authError);

/**
 * Logs the user in with their PIN, if necessary.
 */
Status
cacheLoginPin(std::shared_ptr<Login> &result,
              const char *szUserName, const std::string pin,
              AuthError &authError);

/**
 * Logs the user in with their decryption key, if necessary.
 */
Status
cacheLoginKey(std::shared_ptr<Login> &result,
              const char *szUserName, DataSlice key);

/**
 * Retrieves the cached login, assuming the username still matches.
 */
Status
cacheLogin(std::shared_ptr<Login> &result, const char *szUserName);

/**
 * Retrieves the cached account, assuming the username still matches.
 */
Status
cacheAccount(std::shared_ptr<Account> &result, const char *szUserName);

/**
 * Creates a new wallet and adds it to the cache.
 */
Status
cacheWalletNew(std::shared_ptr<Wallet> &result, const char *szUserName,
               const std::string &name, int currency);

/**
 * Retrieves a wallet for the currently logged-in user.
 * Verifies that the passed-in wallet id is not a null pointer.
 */
Status
cacheWallet(std::shared_ptr<Wallet> &result, const char *szUserName,
            const char *szUUID);

/**
 * Removes a wallet from file and cache.
 */
Status
cacheWalletRemove(const char *szUserName, const char *szUUID);

} // namespace abcd

#endif
