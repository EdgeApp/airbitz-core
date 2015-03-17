/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginShim.hpp"
#include "../abcd/login/Lobby.hpp"
#include "../abcd/login/Login.hpp"
#include "../abcd/login/LoginDir.hpp"
#include "../abcd/login/LoginPassword.hpp"
#include "../abcd/login/LoginPin.hpp"
#include "../abcd/login/LoginRecovery.hpp"
#include "../abcd/login/LoginServer.hpp"

namespace abcd {

// We cache a single account, which is fine for the UI's needs:
std::mutex gLoginMutex;
Lobby *gLobbyCache = nullptr;
Login *gLoginCache = nullptr;

/**
 * Clears the cached login.
 * The caller should already be holding the login mutex.
 */
static void
cacheClear()
{
    delete gLobbyCache;
    delete gLoginCache;
    gLobbyCache = nullptr;
    gLoginCache = nullptr;
}

void
cacheLogout()
{
   std::lock_guard<std::mutex> lock(gLoginMutex);
   cacheClear();
}

Status
cacheLobby(const char *szUserName)
{
    // Clear the cache if the username has changed:
    if (gLobbyCache)
    {
        std::string fixed;
        if (!Lobby::fixUsername(fixed, szUserName) ||
            gLobbyCache->username() != fixed)
        {
            cacheClear();
        }
    }

    // Load the new lobby, if necessary:
    if (!gLobbyCache)
    {
        gLobbyCache = new Lobby();
        ABC_CHECK(gLobbyCache->init(szUserName));
    }

    return Status();
}

Status
cacheLoginNew(const char *szUserName, const char *szPassword)
{
    // Ensure that the username hasn't changed:
    ABC_CHECK(cacheLobby(szUserName));

    // Log the user in, if necessary:
    if (!gLoginCache)
    {
        ABC_CHECK_OLD(ABC_LoginCreate(gLoginCache, gLobbyCache, szPassword, &error));
        ABC_CHECK(gLoginCache->syncDirCreate());
    }

    return Status();
}

Status
cacheLoginPassword(const char *szUserName, const char *szPassword)
{
    // Ensure that the username hasn't changed:
    ABC_CHECK(cacheLobby(szUserName));

    // Log the user in, if necessary:
    if (!gLoginCache)
    {
        if (!szPassword)
            return ABC_ERROR(ABC_CC_NULLPtr, "Not logged in");

        ABC_CHECK_OLD(ABC_LoginPassword(gLoginCache, gLobbyCache, szPassword, &error));
        ABC_CHECK(gLoginCache->syncDirCreate());
    }

    return Status();
}

Status
cacheLoginRecovery(const char *szUserName, const char *szRecoveryAnswers)
{
    // Ensure that the username hasn't changed:
    ABC_CHECK(cacheLobby(szUserName));

    // Log the user in, if necessary:
    if (!gLoginCache)
    {
        ABC_CHECK_OLD(ABC_LoginRecovery(gLoginCache, gLobbyCache, szRecoveryAnswers, &error));
        ABC_CHECK(gLoginCache->syncDirCreate());
    }

    return Status();
}

Status
cacheLoginPin(const char *szUserName, const char *szPin)
{
    // Ensure that the username hasn't changed:
    ABC_CHECK(cacheLobby(szUserName));

    // Log the user in, if necessary:
    if (!gLoginCache)
    {
        ABC_CHECK_OLD(ABC_LoginPin(gLoginCache, gLobbyCache, szPin, &error));
        ABC_CHECK(gLoginCache->syncDirCreate());
    }

    return Status();
}

/**
 * Obtains the information needed to access the sync dir for a given account.
 *
 * @param szUserName UserName for the account to access
 * @param szPassword Password for the account to access
 * @param ppKeys     Location to store returned pointer. The caller must free the structure.
 * @param pError     A pointer to the location to store the error if there is one
 */
tABC_CC ABC_LoginShimGetSyncKeys(const char *szUserName,
                                 const char *szPassword,
                                 tABC_SyncKeys **ppKeys,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(ppKeys);

    // Log the user in, if necessary:
    ABC_CHECK_NEW(cacheLoginPassword(szUserName, szPassword), pError);

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(*gLoginCache, ppKeys, pError));

exit:
    return cc;
}

/**
 * Obtains the information needed to access the server for a given account.
 *
 * @param szUserName UserName for the account to access
 * @param szPassword Password for the account to access
 * @param pL1        A buffer to receive L1. The caller must free this.
 * @param pLP1       A buffer to receive LP1. The caller must free this.
 * @param pError     A pointer to the location to store the error if there is one
 */

tABC_CC ABC_LoginShimGetServerKeys(const char *szUserName,
                                   const char *szPassword,
                                   tABC_U08Buf *pL1,
                                   tABC_U08Buf *pLP1,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);

    // Log the user in, if necessary:
    ABC_CHECK_NEW(cacheLoginPassword(szUserName, szPassword), pError);

    ABC_CHECK_RET(ABC_LoginGetServerKeys(*gLoginCache, pL1, pLP1, pError));

exit:
    return cc;
}

} // namespace abcd
