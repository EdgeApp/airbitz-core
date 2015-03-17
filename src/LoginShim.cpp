/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginShim.hpp"
#include "../abcd/General.hpp"
#include "../abcd/Wallet.hpp"
#include "../abcd/login/Lobby.hpp"
#include "../abcd/login/Login.hpp"
#include "../abcd/login/LoginDir.hpp"
#include "../abcd/login/LoginPassword.hpp"
#include "../abcd/login/LoginPin.hpp"
#include "../abcd/login/LoginRecovery.hpp"
#include "../abcd/login/LoginServer.hpp"
#include "../abcd/util/Util.hpp"

namespace abcd {

// We cache a single account, which is fine for the UI's needs:
std::mutex gLoginMutex;
Lobby *gLobbyCache = nullptr;
Login *gLoginCache = nullptr;

/**
 * Clears the cached login.
 * The caller should already be holding the login mutex.
 */
void
cacheClear()
{
    delete gLobbyCache;
    delete gLoginCache;
    gLobbyCache = nullptr;
    gLoginCache = nullptr;
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
cacheLogin(const char *szUserName, const char *szPassword)
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

void ABC_LoginShimLogout()
{
    std::lock_guard<std::mutex> lock(gLoginMutex);
    cacheClear();
}

/**
 * Signs into an account
 * This cache's the keys for an account
 */
tABC_CC ABC_LoginShimLogin(const char *szUserName,
                           const char *szPassword,
                           tABC_Error *pError)
{
    tABC_Error error;
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);

    ABC_CHECK_NEW(cacheLogin(szUserName, szPassword), pError);

    // Take this non-blocking opportunity to update the general info:
    ABC_GeneralUpdateInfo(&error);

exit:
    return cc;
}

/**
 * Create an account
 */
tABC_CC ABC_LoginShimNewAccount(const char *szUserName,
                                const char *szPassword,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);

    cacheClear();
    ABC_CHECK_NEW(cacheLobby(szUserName), pError);
    ABC_CHECK_RET(ABC_LoginCreate(gLoginCache, gLobbyCache, szPassword, pError));
    ABC_CHECK_NEW(gLoginCache->syncDirCreate(), pError);

    // Take this non-blocking opportunity to update the general info:
    ABC_CHECK_RET(ABC_GeneralUpdateQuestionChoices(pError));
    ABC_CHECK_RET(ABC_GeneralUpdateInfo(pError));

exit:
    return cc;
}

/**
 * Set the recovery questions for an account
 *
 * This function sets the password recovery informatin for the account.
 * This includes sending a new care package to the server
 *
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_LoginShimSetRecovery(const char *szUserName,
                                 const char *szPassword,
                                 const char *szRecoveryQuestions,
                                 const char *szRecoveryAnswers,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);

    // Log the user in, if necessary:
    ABC_CHECK_NEW(cacheLogin(szUserName, szPassword), pError);

    // Do the change:
    ABC_CHECK_RET(ABC_LoginRecoverySet(*gLoginCache,
        szRecoveryQuestions, szRecoveryAnswers, pError));

exit:
    return cc;
}

/**
 * Change password for an account
 *
 * This function sets the password recovery informatin for the account.
 * This includes sending a new care package to the server
 *
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_LoginShimSetPassword(const char *szUserName,
                                 const char *szPassword,
                                 const char *szRecoveryAnswers,
                                 const char *szNewPassword,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);

    // Ensure that the username hasn't changed:
    ABC_CHECK_NEW(cacheLobby(szUserName), pError);

    // Log the user in, if necessary:
    if (!gLoginCache)
    {
        if (szPassword)
        {
            ABC_CHECK_RET(ABC_LoginPassword(gLoginCache, gLobbyCache, szPassword, pError));
        }
        else
        {
            ABC_CHECK_RET(ABC_LoginRecovery(gLoginCache, gLobbyCache, szRecoveryAnswers, pError));
        }
        ABC_CHECK_NEW(gLoginCache->syncDirCreate(), pError);
    }

    ABC_CHECK_RET(ABC_LoginPasswordSet(*gLoginCache, szNewPassword, pError));
    ABC_WalletClearCache();

exit:
    return cc;
}

/**
 * Check that the recovery answers for a given account are valid
 * @param pbValid true is stored in here if they are correct
 */
tABC_CC ABC_LoginShimCheckRecovery(const char *szUserName,
                                   const char *szRecoveryAnswers,
                                   bool *pbValid,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);
    Login *login = nullptr;

    ABC_CHECK_NEW(cacheLobby(szUserName), pError);
    cc = ABC_LoginRecovery(login, gLobbyCache, szRecoveryAnswers, pError);

    if (ABC_CC_Ok == cc)
    {
        // Yup! That was it:
        if (!gLoginCache)
        {
            gLoginCache = login;
            login = nullptr;
            ABC_CHECK_NEW(gLoginCache->syncDirCreate(), pError);
        }
        *pbValid = true;
    }
    else if (ABC_CC_DecryptFailure == cc)
    {
        // The answers didn't match, which is OK:
        ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    }

exit:
    delete login;

    return cc;
}

/**
 * Logs in using the PIN-based mechanism.
 */
tABC_CC ABC_LoginShimPinLogin(const char *szUserName,
                              const char *szPin,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);

    cacheClear();
    ABC_CHECK_NEW(cacheLobby(szUserName), pError);
    ABC_CHECK_RET(ABC_LoginPin(gLoginCache, gLobbyCache, szPin, pError));
    ABC_CHECK_NEW(gLoginCache->syncDirCreate(), pError);

exit:
    return cc;
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
    ABC_CHECK_NEW(cacheLogin(szUserName, szPassword), pError);

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
    ABC_CHECK_NEW(cacheLogin(szUserName, szPassword), pError);

    ABC_CHECK_RET(ABC_LoginGetServerKeys(*gLoginCache, pL1, pLP1, pError));

exit:
    return cc;
}

} // namespace abcd
