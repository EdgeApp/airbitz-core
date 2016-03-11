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

#include "../abcd/util/Data.hpp"
#include "../abcd/util/Status.hpp"
#include <memory>

namespace abcd {

class Lobby;
class Login;
class Account;
class Wallet;
struct AuthError;

/**
 * Clears all cached login objects.
 */
void
cacheLogout();

/**
 * Loads the lobby for the given user into the cache.
 * If the username is null, the function returns whatever is cached.
 */
Status
cacheLobby(std::shared_ptr<Lobby> &result, const char *szUserName);

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
 * Logs the user in with their PIN, if necessary.
 */
Status
cacheLoginPin(std::shared_ptr<Login> &result,
              const char *szUserName, const std::string pin,
              AuthError &authError);

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
