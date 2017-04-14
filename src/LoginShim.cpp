/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginShim.hpp"
#include "../abcd/account/Account.hpp"
#include "../abcd/login/Login.hpp"
#include "../abcd/login/LoginPassword.hpp"
#include "../abcd/login/LoginPin.hpp"
#include "../abcd/login/LoginPin2.hpp"
#include "../abcd/login/LoginRecovery.hpp"
#include "../abcd/login/LoginRecovery2.hpp"
#include "../abcd/login/LoginStore.hpp"
#include "../abcd/login/Sharing.hpp"
#include "../abcd/login/json/AuthJson.hpp"
#include "../abcd/login/json/LoginJson.hpp"
#include "../abcd/login/server/LoginServer.hpp"
#include "../abcd/wallet/Wallet.hpp"
#include <map>
#include <mutex>

namespace abcd {

HandleCache<Lobby> gLobbyCache;

// This mutex protects the shared_ptr caches themselves.
// Using a reference count ensures that any objects still in use
// on another thread will not be destroyed during a cache update.
// The mutex only needs to be locked when updating the cache,
// not when using the objects inside.
// The cached objects must provide their own thread safety.
static std::mutex gLoginMutex;
static std::shared_ptr<LoginStore> gLoginStoreCache;
static std::shared_ptr<Login> gLoginCache;
static std::shared_ptr<Account> gAccountCache;
static std::map<std::string, std::shared_ptr<Wallet>> gWalletCache;

/**
 * Clears the cached login.
 * The caller should already be holding the login mutex.
 */
static void
cacheClear()
{
    gLoginStoreCache.reset();
    gLoginCache.reset();
    gAccountCache.reset();
    gWalletCache.clear();
}

void
cacheLogout()
{
    std::lock_guard<std::mutex> lock(gLoginMutex);
    cacheClear();
}

Status
cacheLoginStore(std::shared_ptr<LoginStore> &result, const char *szUserName)
{
    std::lock_guard<std::mutex> lock(gLoginMutex);

    // Clear the cache if the username has changed:
    if (szUserName && gLoginStoreCache)
    {
        std::string fixed;
        ABC_CHECK(LoginStore::fixUsername(fixed, szUserName));
        if (gLoginStoreCache->username() != fixed)
            cacheClear();
    }

    // Load the new store, if necessary:
    if (!gLoginStoreCache)
    {
        if (!szUserName)
            return ABC_ERROR(ABC_CC_NULLPtr, "No user name");
        ABC_CHECK(LoginStore::create(gLoginStoreCache, szUserName));
    }

    result = gLoginStoreCache;
    return Status();
}

Status
cacheLoginNew(std::shared_ptr<Login> &result,
              const char *szUserName, const char *szPassword)
{
    std::shared_ptr<LoginStore> store;
    ABC_CHECK(cacheLoginStore(store, szUserName));

    // Log the user in, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
    {
        ABC_CHECK(Login::createNew(gLoginCache, *store, szPassword));
    }

    result = gLoginCache;
    return Status();
}

Status
cacheLoginPassword(std::shared_ptr<Login> &result,
                   const char *szUserName, const std::string &password,
                   AuthError &authError)
{
    std::shared_ptr<LoginStore> store;
    ABC_CHECK(cacheLoginStore(store, szUserName));

    // Log the user in, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
    {
        ABC_CHECK(loginPassword(gLoginCache, *store, password, authError));
    }

    result = gLoginCache;
    return Status();
}

Status
cacheLoginRecovery(std::shared_ptr<Login> &result,
                   const char *szUserName, const std::string &recoveryAnswers,
                   AuthError &authError)
{
    std::shared_ptr<LoginStore> store;
    ABC_CHECK(cacheLoginStore(store, szUserName));

    // Log the user in, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
    {
        ABC_CHECK(loginRecovery(gLoginCache, *store, recoveryAnswers,
                                authError));
    }

    result = gLoginCache;
    return Status();
}

Status
cacheLoginRecovery2(std::shared_ptr<Login> &result,
                    const char *szUserName, DataSlice recovery2Key,
                    const std::list<std::string> &answers,
                    AuthError &authError)
{
    std::shared_ptr<LoginStore> store;
    ABC_CHECK(cacheLoginStore(store, szUserName));

    // Log the user in, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
    {
        ABC_CHECK(loginRecovery2(gLoginCache, *store, recovery2Key, answers,
                                 authError));
    }

    result = gLoginCache;
    return Status();
}

Status
cacheLoginPin(std::shared_ptr<Login> &result,
              const char *szUserName, const std::string pin,
              AuthError &authError)
{
    std::shared_ptr<LoginStore> store;
    ABC_CHECK(cacheLoginStore(store, szUserName));

    // Log the user in, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
    {
        AccountPaths paths;
        ABC_CHECK(store->paths(paths));
        DataChunk pin2Key;
        if (loginPin2Key(pin2Key, paths))
        {
            // Always use PIN login v2 if we have it:
            ABC_CHECK(loginPin2(gLoginCache, *store, pin2Key, pin, authError));
        }
        else
        {
            // Otherwise try PIN login v1:
            ABC_CHECK(loginPin(gLoginCache, *store, pin, authError));

            // Upgrade to PIN login v2:
            ABC_CHECK(gLoginCache->update());
            ABC_CHECK(loginPin2Set(pin2Key, *gLoginCache, pin));
            ABC_CHECK(loginPinDelete(*store));
        }
    }

    result = gLoginCache;
    return Status();
}

Status
cacheLoginKey(std::shared_ptr<Login> &result,
              const char *szUserName, DataSlice key)
{
    std::shared_ptr<LoginStore> store;
    ABC_CHECK(cacheLoginStore(store, szUserName));

    // Log the user in, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
    {
        ABC_CHECK(Login::createOffline(gLoginCache, *store, key));
    }

    result = gLoginCache;
    return Status();
}

Status
cacheLogin(std::shared_ptr<Login> &result, const char *szUserName)
{
    std::shared_ptr<LoginStore> store;
    ABC_CHECK(cacheLoginStore(store, szUserName));

    // Verify that the user is logged in:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
        return ABC_ERROR(ABC_CC_AccountDoesNotExist, "Not logged in");

    result = gLoginCache;
    return Status();
}

Status
cacheAccount(std::shared_ptr<Account> &result, const char *szUserName)
{
    std::shared_ptr<Login> login;
    ABC_CHECK(cacheLogin(login, szUserName));

    // Create the object, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gAccountCache)
        ABC_CHECK(Account::create(gAccountCache, *login));

    result = gAccountCache;
    return Status();
}

Status
cacheWalletNew(std::shared_ptr<Wallet> &result, const char *szUserName,
               const std::string &name, int currency)
{
    std::shared_ptr<Account> account;
    ABC_CHECK(cacheAccount(account, szUserName));

    // Create the wallet:
    std::shared_ptr<Wallet> out;
    ABC_CHECK(Wallet::createNew(out, *account, name, currency));

    // Add to the cache:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    gWalletCache[out->id()] = out;

    result = std::move(out);
    return Status();
}

Status
cacheWallet(std::shared_ptr<Wallet> &result, const char *szUserName,
            const char *szUUID)
{
    std::shared_ptr<Account> account;
    ABC_CHECK(cacheAccount(account, szUserName));

    if (!szUUID)
        return ABC_ERROR(ABC_CC_NULLPtr, "No wallet id");
    std::string id = szUUID;

    // Try to return the wallet from the cache:
    {
        std::lock_guard<std::mutex> lock(gLoginMutex);
        auto i = gWalletCache.find(id);
        if (i != gWalletCache.end())
        {
            result = i->second;
            return Status();
        }
    }

    // Load the wallet:
    std::shared_ptr<Wallet> out;
    ABC_CHECK(Wallet::create(out, *account, id));

    // Add to the cache:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    gWalletCache[id] = out;

    result = std::move(out);
    return Status();
}

Status
cacheWalletRemove(const char *szUserName, const char *szUUID)
{
    std::shared_ptr<Account> account;
    ABC_CHECK(cacheAccount(account, szUserName));

    if (!szUUID)
        return ABC_ERROR(ABC_CC_NULLPtr, "No wallet id");
    std::string id = szUUID;

    // remove the wallet from the cache:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    auto i = gWalletCache.find(id);
    if (i != gWalletCache.end())
    {
        ABC_CHECK(account->wallets.remove(id));
        gWalletCache.erase(i);
    }
    return Status();
}

std::shared_ptr<Wallet>
cacheWalletSoft(const std::string &id)
{
    // Try to return the wallet from the cache:
    {
        std::lock_guard<std::mutex> lock(gLoginMutex);
        auto i = gWalletCache.find(id);
        if (i != gWalletCache.end())
        {
            return i->second;
        }
    }

    // Nothing matched:
    return nullptr;
}

} // namespace abcd
