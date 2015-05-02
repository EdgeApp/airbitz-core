/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginShim.hpp"
#include "../abcd/account/Account.hpp"
#include "../abcd/login/Lobby.hpp"
#include "../abcd/login/Login.hpp"
#include "../abcd/login/LoginDir.hpp"
#include "../abcd/login/LoginPassword.hpp"
#include "../abcd/login/LoginPin.hpp"
#include "../abcd/login/LoginRecovery.hpp"
#include "../abcd/login/LoginServer.hpp"
#include <mutex>

namespace abcd {

// This mutex protects the shared_ptr caches themselves.
// Using a reference count ensures that any objects still in use
// on another thread will not be destroyed during a cache update.
// The mutex only needs to be locked when updating the cache,
// not when using the objects inside.
// The cached objects must provide their own thread safety.
static std::mutex gLoginMutex;
static std::shared_ptr<Lobby> gLobbyCache;
static std::shared_ptr<Login> gLoginCache;
static std::shared_ptr<Account> gAccountCache;

/**
 * Clears the cached login.
 * The caller should already be holding the login mutex.
 */
static void
cacheClear()
{
    gLobbyCache.reset();
    gLoginCache.reset();
    gAccountCache.reset();
}

void
cacheLogout()
{
   std::lock_guard<std::mutex> lock(gLoginMutex);
   cacheClear();
}

Status
cacheLobby(std::shared_ptr<Lobby> &result, const char *szUserName)
{
    std::lock_guard<std::mutex> lock(gLoginMutex);

    // Clear the cache if the username has changed:
    if (szUserName && gLobbyCache)
    {
        std::string fixed;
        ABC_CHECK(Lobby::fixUsername(fixed, szUserName));
        if (gLobbyCache->username() != fixed)
            cacheClear();
    }

    // Load the new lobby, if necessary:
    if (!gLobbyCache)
    {
        if (!szUserName)
            return ABC_ERROR(ABC_CC_NULLPtr, "No user name");
        std::unique_ptr<Lobby> lobby(new Lobby());
        ABC_CHECK(lobby->init(szUserName));
        gLobbyCache.reset(lobby.release());
    }

    result = gLobbyCache;
    return Status();
}

Status
cacheLoginNew(std::shared_ptr<Login> &result,
    const char *szUserName, const char *szPassword)
{
    std::shared_ptr<Lobby> lobby;
    ABC_CHECK(cacheLobby(lobby, szUserName));

    // Log the user in, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
    {
        ABC_CHECK_OLD(ABC_LoginCreate(gLoginCache, lobby, szPassword, &error));
    }

    result = gLoginCache;
    return Status();
}

Status
cacheLoginPassword(std::shared_ptr<Login> &result,
    const char *szUserName, const char *szPassword)
{
    std::shared_ptr<Lobby> lobby;
    ABC_CHECK(cacheLobby(lobby, szUserName));

    // Log the user in, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
    {
        ABC_CHECK_OLD(ABC_LoginPassword(gLoginCache, lobby, szPassword, &error));
    }

    result = gLoginCache;
    return Status();
}

Status
cacheLoginRecovery(std::shared_ptr<Login> &result,
    const char *szUserName, const char *szRecoveryAnswers)
{
    std::shared_ptr<Lobby> lobby;
    ABC_CHECK(cacheLobby(lobby, szUserName));

    // Log the user in, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
    {
        ABC_CHECK_OLD(ABC_LoginRecovery(gLoginCache, lobby, szRecoveryAnswers, &error));
    }

    result = gLoginCache;
    return Status();
}

Status
cacheLoginPin(std::shared_ptr<Login> &result,
    const char *szUserName, const char *szPin)
{
    std::shared_ptr<Lobby> lobby;
    ABC_CHECK(cacheLobby(lobby, szUserName));

    // Log the user in, if necessary:
    std::lock_guard<std::mutex> lock(gLoginMutex);
    if (!gLoginCache)
    {
        ABC_CHECK_OLD(ABC_LoginPin(gLoginCache, lobby, szPin, &error));
    }

    result = gLoginCache;
    return Status();
}

Status
cacheLogin(std::shared_ptr<Login> &result, const char *szUserName)
{
    std::shared_ptr<Lobby> lobby;
    ABC_CHECK(cacheLobby(lobby, szUserName));

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
    {
        std::unique_ptr<Account> account(new Account(login));
        ABC_CHECK(account->init());
        gAccountCache.reset(account.release());
    }

    result = gAccountCache;
    return Status();
}

} // namespace abcd
