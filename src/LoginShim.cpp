/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginShim.hpp"
#include "../abcd/General.hpp"
#include "../abcd/Wallet.hpp"
#include "../abcd/login/Login.hpp"
#include "../abcd/login/LoginDir.hpp"
#include "../abcd/login/LoginPassword.hpp"
#include "../abcd/login/LoginPin.hpp"
#include "../abcd/login/LoginRecovery.hpp"
#include "../abcd/login/LoginServer.hpp"
#include "../abcd/util/Util.hpp"
#include <mutex>

namespace abcd {

// We cache a single login object, which is fine for the UI's needs:
tABC_Login *gLoginCache = NULL;
std::mutex gLoginMutex;

static void ABC_LoginCacheClear();
static void ABC_LoginCacheClearOther(const char *szUserName);
static tABC_CC ABC_LoginCacheObject(const char *szUserName, const char *szPassword, tABC_Error *pError);

/**
 * Clears the cached login object.
 * The caller should already be holding the login mutex.
 */
static
void ABC_LoginCacheClear()
{
    ABC_LoginFree(gLoginCache);
    gLoginCache = NULL;
}

/**
 * Clears the cache if the current object doesn't match the given username.
 * The caller should already be holding the login mutex.
 */
static
void ABC_LoginCacheClearOther(const char *szUserName)
{
    if (gLoginCache)
    {
        tABC_Error error;
        int match = 0;

        ABC_LoginCheckUserName(gLoginCache, szUserName, &match, &error);
        if (!match)
            ABC_LoginCacheClear();
    }
}

/**
 * Loads the account for the given user into the login object cache.
 * The caller should already be holding the login mutex.
 */
static
tABC_CC ABC_LoginCacheObject(const char *szUserName,
                             const char *szPassword,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    // Clear the cache if it has the wrong object:
    ABC_LoginCacheClearOther(szUserName);

    // Load the right object, if necessary:
    if (!gLoginCache)
    {
        ABC_CHECK_ASSERT(szPassword, ABC_CC_NULLPtr, "Not logged in");
        ABC_CHECK_RET(ABC_LoginPassword(&gLoginCache, szUserName, szPassword, pError));
        ABC_CHECK_RET(ABC_LoginDirMakeSyncDir(gLoginCache->AccountNum, gLoginCache->szSyncKey, pError));
    }

exit:
    return cc;
}

/**
 * Clears all the keys from the cache.
 */
void ABC_LoginShimLogout()
{
    std::lock_guard<std::mutex> lock(gLoginMutex);
    ABC_LoginCacheClear();
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

    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

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

    ABC_LoginCacheClear();
    ABC_CHECK_RET(ABC_LoginCreate(szUserName, szPassword, &gLoginCache, pError));
    ABC_CHECK_RET(ABC_LoginDirMakeSyncDir(gLoginCache->AccountNum, gLoginCache->szSyncKey, pError));

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

    // Load the account into the cache:
    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    // Do the change:
    ABC_CHECK_RET(ABC_LoginRecoverySet(gLoginCache,
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

    // Clear the cache if it has the wrong object:
    ABC_LoginCacheClearOther(szUserName);

    // Load the right object, if necessary:
    if (!gLoginCache)
    {
        if (szPassword)
        {
            ABC_CHECK_RET(ABC_LoginPassword(&gLoginCache, szUserName, szPassword, pError));
            ABC_CHECK_RET(ABC_LoginDirMakeSyncDir(gLoginCache->AccountNum, gLoginCache->szSyncKey, pError));
        }
        else
        {
            ABC_CHECK_RET(ABC_LoginRecovery(&gLoginCache, szUserName, szRecoveryAnswers, pError));
            ABC_CHECK_RET(ABC_LoginDirMakeSyncDir(gLoginCache->AccountNum, gLoginCache->szSyncKey, pError));
        }
    }

    // Do the change:
    ABC_CHECK_RET(ABC_LoginPasswordSet(gLoginCache, szNewPassword, pError));

    // Clear wallet cache
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
    tABC_Login *pObject = NULL;

    cc = ABC_LoginRecovery(&pObject, szUserName, szRecoveryAnswers, pError);

    if (ABC_CC_Ok == cc)
    {
        // Yup! That was it:
        ABC_LoginCacheClear();
        gLoginCache = pObject;
        *pbValid = true;
        ABC_CHECK_RET(ABC_LoginDirMakeSyncDir(gLoginCache->AccountNum, gLoginCache->szSyncKey, pError));
    }
    else if (ABC_CC_DecryptFailure == cc)
    {
        // The answers didn't match, which is OK:
        ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    }

exit:
    return cc;
}

/**
 * Logs in using the PIN-based mechanism.
 */
tABC_CC ABC_LoginShimPinLogin(const char *szUserName,
                              const char *szPIN,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);
    tABC_Login *pObject = NULL;

    ABC_CHECK_RET(ABC_LoginPin(&pObject, szUserName, szPIN, pError));
    ABC_LoginCacheClear();
    gLoginCache = pObject;

    ABC_CHECK_RET(ABC_LoginDirMakeSyncDir(gLoginCache->AccountNum, gLoginCache->szSyncKey, pError));

exit:
    return cc;
}

/**
 * Sets up a PIN login package, both on-disk and on the server.
 */
tABC_CC ABC_LoginShimPinSetup(const char *szUserName,
                              const char *szPassword,
                              const char *szPIN,
                              time_t expires,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);

    // Load the account into the cache:
    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    ABC_CHECK_RET(ABC_LoginPinSetup(gLoginCache, szPIN, expires, pError));

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

    // Load the account into the cache:
    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(gLoginCache, ppKeys, pError));

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

    // Load the account into the cache:
    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    ABC_CHECK_RET(ABC_LoginGetServerKeys(gLoginCache, pL1, pLP1, pError));

exit:
    return cc;
}

/**
 * Validates that the provided password is correct.
 * This is used in the GUI to guard access to certain actions.
 */
tABC_CC ABC_LoginShimPasswordOk(const char *szUserName,
                                const char *szPassword,
                                bool *pOk,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::lock_guard<std::mutex> lock(gLoginMutex);

    // Load the account into the cache:
    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    ABC_CHECK_RET(ABC_LoginPasswordOk(gLoginCache, szPassword, pOk, pError));

exit:
    return cc;
}

} // namespace abcd
