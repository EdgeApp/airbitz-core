/**
 * @file
 * AirBitz file-sync functions
 *
 * See LICENSE for copy, modification, and use permissions
 *
 * @author See AUTHORS
 * @version 1.0
 */

#include "ABC_Sync.h"
#include "ABC_Util.h"
#include "ABC_Mutex.h"
#include "sync.h"

#include <pthread.h>

static bool gbInitialized = false;
static pthread_mutex_t gMutex;

static tABC_CC ABC_SyncMutexLock(tABC_Error *pError);
static tABC_CC ABC_SyncMutexUnlock(tABC_Error *pError);
static char *gszCaCertPath = NULL;

/**
 * Logs error information produced by libgit2.
 */
static void SyncLogGitError(int e)
{
    const git_error *error = giterr_last();
    if (error && error->message)
    {
        ABC_DebugLog("libgit2 returned %d: %s", e, error->message);
    }
    else
    {
        ABC_DebugLog("libgit2 returned %d: <no message>", e);
    }
}

static int SyncMaster(git_repository *repo, int *dirty, int *need_push)
{
    int e;
    tABC_CC cc;

    ABC_CHECK_RET(ABC_MutexLock(NULL));
    e = sync_master(repo, dirty, need_push);
exit:
    ABC_MutexUnlock(NULL);
    return e;
}

/**
 * Frees a tABC_SyncKeys structure. While the keys inside the structure do
 * not need to be freed, the string and the structure itself certainly do.
 */
void ABC_SyncFreeKeys(tABC_SyncKeys *pKeys)
{
    if (pKeys)
    {
        ABC_FREE_STR(pKeys->szSyncDir);
        ABC_CLEAR_FREE(pKeys, sizeof(tABC_SyncKeys));
    }
}

/**
 * Initializes the underlying git library. Should be called at program start.
 */
tABC_CC ABC_SyncInit(const char *szCaCertPath, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "ABC_Sync has already been initalized");

    pthread_mutexattr_t mutexAttrib;
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_init(&mutexAttrib), ABC_CC_MutexError, "ABC_Sync could not create mutex attribute");
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_settype(&mutexAttrib, PTHREAD_MUTEX_RECURSIVE), ABC_CC_MutexError, "ABC_Sync could not set mutex attributes");
    ABC_CHECK_ASSERT(0 == pthread_mutex_init(&gMutex, &mutexAttrib), ABC_CC_MutexError, "ABC_Sync could not create mutex");
    pthread_mutexattr_destroy(&mutexAttrib);

    gbInitialized = true;

    e = git_threads_init();
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "git_threads_init failed");

    if (szCaCertPath)
    {
        ABC_STRDUP(gszCaCertPath, szCaCertPath);
    }
exit:
    if (e < 0) SyncLogGitError(e);
    return cc;
}

/**
 * Shuts down the underlying git library. Should be called when the program
 * exits.
 */
void ABC_SyncTerminate()
{
    git_threads_shutdown();

    if (gbInitialized == true)
    {
        pthread_mutex_destroy(&gMutex);

        gbInitialized = false;
    }
    ABC_FREE_STR(gszCaCertPath);
}

/**
 * Prepares a directory for syncing. This must be called one time after
 * the directory has first been created.
 */
tABC_CC ABC_SyncMakeRepo(const char *szRepoPath,
                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    git_repository *repo = NULL;

    ABC_CHECK_RET(ABC_SyncMutexLock(pError));

    e = git_repository_init(&repo, szRepoPath, 0);
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "git_repository_init failed");

exit:
    if (e < 0) SyncLogGitError(e);
    if (repo) git_repository_free(repo);

    ABC_CHECK_RET(ABC_SyncMutexUnlock(pError));
    return cc;
}

/**
 * Synchronizes the directory with the server. New files in the folder will
 * go up to the server, and new files on the server will come down to the
 * directory. If there is a conflict, the server's file will win.
 * @param pDirty set to 1 if the sync has modified the filesystem, or 0
 * otherwise.
 */
tABC_CC ABC_SyncRepo(const char *szRepoPath,
                     const char *szServer,
                     int *pDirty,
                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    git_repository *repo = NULL;
    git_config *cfg = NULL;
    int dirty, need_push;

    ABC_CHECK_RET(ABC_SyncMutexLock(pError));

    e = git_repository_open(&repo, szRepoPath);
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "git_repository_open failed");

    e = git_repository_config(&cfg, repo);
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "git_repository_config failed");

    if (gszCaCertPath)
    {
        e = git_config_set_string(cfg, "http.sslcainfo", gszCaCertPath);
        ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "http.sslcainfo failed");
    }

    e = sync_fetch(repo, szServer);
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "sync_fetch failed");

    e = SyncMaster(repo, &dirty, &need_push);
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "sync_master failed");

    if (need_push)
    {
        e = sync_push(repo, szServer);
        ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "sync_push failed");
    }

    *pDirty = dirty;

exit:
    if (e < 0) SyncLogGitError(e);
    if (repo) git_repository_free(repo);

    ABC_CHECK_RET(ABC_SyncMutexUnlock(pError));

    return cc;
}

static
tABC_CC ABC_SyncMutexLock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_Sync has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_lock(&gMutex), ABC_CC_MutexError, "ABC_Sync error locking mutex");

exit:

    return cc;
}

static
tABC_CC ABC_SyncMutexUnlock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_Sync has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_unlock(&gMutex), ABC_CC_MutexError, "ABC_Sync error unlocking mutex");

exit:

    return cc;
}
