/**
 * @file
 * AirBitz Login functions.
 *
 * This file wrapps the methods of `ABC_LoginObject.h` with a caching layer
 * for backwards-compatibility with the old API.
 */

#include "ABC_Login.h"
#include "ABC_LoginObject.h"
#include "ABC_General.h"
#include "ABC_Wallet.h"
#include "util/ABC_Mutex.h"
#include "util/ABC_Util.h"

// We cache a single login object, which is fine for the UI's needs:
tABC_LoginObject *gLoginCache = NULL;

static void ABC_LoginCacheClear();
static void ABC_LoginCacheClearOther(const char *szUserName);
static tABC_CC ABC_LoginCacheObject(const char *szUserName, const char *szPassword, tABC_Error *pError);
static tABC_CC ABC_LoginMutexLock(tABC_Error *pError);
static tABC_CC ABC_LoginMutexUnlock(tABC_Error *pError);

/**
 * Clears the cached login object.
 * The caller should already be holding the login mutex.
 */
static
void ABC_LoginCacheClear()
{
    ABC_LoginObjectFree(gLoginCache);
    gLoginCache = NULL;
}

/**
 * Clears the cache if the current object doesn't match the given username.
 */
static
void ABC_LoginCacheClearOther(const char *szUserName)
{
    if (gLoginCache)
    {
        tABC_Error error;
        int match = 0;

        ABC_LoginObjectCheckUserName(gLoginCache, szUserName, &match, &error);
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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);

    // Clear the cache if it has the wrong object:
    ABC_LoginCacheClearOther(szUserName);

    // Load the right object, if necessary:
    if (!gLoginCache)
    {
        ABC_CHECK_RET(ABC_LoginObjectFromPassword(szUserName, szPassword,
            &gLoginCache, pError));
    }

exit:
    return cc;
}

/**
 * Clears all the keys from the cache.
 */
tABC_CC ABC_LoginClearKeyCache(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_CHECK_RET(ABC_LoginMutexLock(pError));

    ABC_LoginCacheClear();

exit:
    ABC_LoginMutexUnlock(NULL);
    return cc;
}

/**
 * Checks if the username and password are valid.
 *
 * If the login info is valid, the keys for this account
 * are also cached.
 * If the creditials are not valid, an error will be returned
 *
 * @param szUserName UserName for validation
 * @param szPassword Password for validation
 */
tABC_CC ABC_LoginCheckCredentials(const char *szUserName,
                                  const char *szPassword,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_RET(ABC_LoginMutexLock(pError));

    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

exit:
    ABC_LoginMutexUnlock(NULL);

    return cc;
}

/**
 * Signs into an account
 * This cache's the keys for an account
 */
tABC_CC ABC_LoginSignIn(const char *szUserName,
                        const char *szPassword,
                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_RET(ABC_LoginMutexLock(pError));

    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    // Take this non-blocking opportunity to update the general info:
    ABC_CHECK_RET(ABC_GeneralUpdateInfo(pError));

exit:
    ABC_LoginMutexUnlock(NULL);

    return cc;
}

/**
 * Create an account
 */
tABC_CC ABC_LoginCreate(const char *szUserName,
                        const char *szPassword,
                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_RET(ABC_LoginMutexLock(pError));

    ABC_LoginCacheClear();
    ABC_CHECK_RET(ABC_LoginObjectCreate(szUserName, szPassword, &gLoginCache, pError));

    // Take this non-blocking opportunity to update the general info:
    ABC_CHECK_RET(ABC_GeneralUpdateQuestionChoices(pError));
    ABC_CHECK_RET(ABC_GeneralUpdateInfo(pError));

exit:
    ABC_LoginMutexUnlock(NULL);

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
tABC_CC ABC_LoginSetRecovery(const char *szUserName,
                             const char *szPassword,
                             const char *szRecoveryQuestions,
                             const char *szRecoveryAnswers,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_RET(ABC_LoginMutexLock(pError));

    // Load the account into the cache:
    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    // Do the change:
    ABC_CHECK_RET(ABC_LoginObjectSetRecovery(gLoginCache,
        szRecoveryQuestions, szRecoveryAnswers, pError));

exit:
    ABC_LoginMutexUnlock(NULL);

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
tABC_CC ABC_LoginChangePassword(const char *szUserName,
                                const char *szPassword,
                                const char *szRecoveryAnswers,
                                const char *szNewPassword,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szNewPassword);
    ABC_CHECK_RET(ABC_LoginMutexLock(pError));

    // Clear the cache if it has the wrong object:
    ABC_LoginCacheClearOther(szUserName);

    // Load the right object, if necessary:
    if (!gLoginCache)
    {
        if (szPassword)
        {
            ABC_CHECK_RET(ABC_LoginObjectFromPassword(szUserName, szPassword,
                &gLoginCache, pError));
        }
        else
        {
            ABC_CHECK_RET(ABC_LoginObjectFromRecovery(szUserName, szRecoveryAnswers,
                &gLoginCache, pError));
        }
    }

    // Do the change:
    ABC_CHECK_RET(ABC_LoginObjectSetPassword(gLoginCache, szNewPassword, pError));

    // Clear wallet cache
    ABC_CHECK_RET(ABC_WalletClearCache(pError));

exit:
    ABC_LoginMutexUnlock(NULL);

    return cc;
}

/**
 * Check that the recovery answers for a given account are valid
 * @param pbValid true is stored in here if they are correct
 */
tABC_CC ABC_LoginCheckRecoveryAnswers(const char *szUserName,
                                      const char *szRecoveryAnswers,
                                      bool *pbValid,
                                      tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_RET(ABC_LoginMutexLock(pError));

    tABC_LoginObject *pObject = NULL;
    cc = ABC_LoginObjectFromRecovery(szUserName, szRecoveryAnswers,
        &pObject, pError);

    if (ABC_CC_Ok == cc)
    {
        // Yup! That was it:
        ABC_LoginCacheClear();
        gLoginCache = pObject;
        *pbValid = true;
    }
    else if (ABC_CC_DecryptFailure == cc)
    {
        // The answers didn't match, which is OK:
        ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    }

exit:
    ABC_LoginMutexUnlock(NULL);

    return cc;
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
tABC_CC ABC_LoginGetRecoveryQuestions(const char *szUserName,
                                      char **pszQuestions,
                                      tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(pszQuestions);

    ABC_CHECK_RET(ABC_LoginObjectGetRQ(szUserName, pszQuestions, pError));

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
tABC_CC ABC_LoginGetSyncKeys(const char *szUserName,
                             const char *szPassword,
                             tABC_SyncKeys **ppKeys,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_RET(ABC_LoginMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(ppKeys);

    // Load the account into the cache:
    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    // Grab the keys:
    ABC_CHECK_RET(ABC_LoginObjectGetSyncKeys(gLoginCache, ppKeys, pError));

exit:
    ABC_LoginMutexUnlock(NULL);

    return cc;
}

/**
 * Obtains the information needed to access the server for a given account.
 *
 * @param szUserName UserName for the account to access
 * @param szPassword Password for the account to access
 * @param pL1        A buffer to receive L1. Do *not* free this.
 * @param pLP1       A buffer to receive LP1. Do *not* free this.
 * @param pError     A pointer to the location to store the error if there is one
 */

tABC_CC ABC_LoginGetServerKeys(const char *szUserName,
                               const char *szPassword,
                               tABC_U08Buf *pL1,
                               tABC_U08Buf *pLP1,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_RET(ABC_LoginMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(pL1);
    ABC_CHECK_NULL(pLP1);

    // Load the account into the cache:
    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    // Grab the keys:
    ABC_CHECK_RET(ABC_LoginObjectGetServerKeys(gLoginCache, pL1, pLP1, pError));

exit:
    ABC_LoginMutexUnlock(NULL);
    return cc;
}

/**
 * Downloads and saves a new LoginPackage from the server.
 */
tABC_CC ABC_LoginUpdateLoginPackageFromServer(const char *szUserName,
                                              const char *szPassword,
                                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_RET(ABC_LoginMutexLock(pError));

    // Load the account into the cache:
    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    // Do the update:
    ABC_CHECK_RET(ABC_LoginObjectUpdateLoginPackage(gLoginCache, pError));

exit:
    ABC_LoginMutexUnlock(NULL);
    return cc;
}

/**
 * Sync the account data
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param szPassword    Password for the account associated with the settings
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_LoginSyncData(const char *szUserName,
                          const char *szPassword,
                          int *pDirty,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);

    // Load the account into the cache:
    ABC_CHECK_RET(ABC_LoginCacheObject(szUserName, szPassword, pError));

    // Do the update:
    ABC_CHECK_RET(ABC_LoginObjectSync(gLoginCache, pDirty, pError));

exit:
    return cc;
}

/**
 * Locks the mutex
 *
 * ABC_Wallet uses the same mutex as ABC_Login so that there will be no situation in
 * which one thread is in ABC_Wallet locked on a mutex and calling a thread safe ABC_Login call
 * that is locked from another thread calling a thread safe ABC_Wallet call.
 * In other words, since they call each other, they need to share a recursive mutex.
 */
static
tABC_CC ABC_LoginMutexLock(tABC_Error *pError)
{
    return ABC_MutexLock(pError);
}

/**
 * Unlocks the mutex
 *
 */
static
tABC_CC ABC_LoginMutexUnlock(tABC_Error *pError)
{
    return ABC_MutexUnlock(pError);
}
