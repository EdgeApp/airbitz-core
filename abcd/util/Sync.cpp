/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Sync.hpp"
#include "Util.hpp"
#include "Mutex.hpp"
#include "../General.hpp"
#include "../../minilibs/git-sync/sync.h"
#include <stdlib.h>
#include <mutex>

namespace abcd {

static bool gbInitialized = false;
std::recursive_mutex gSyncMutex;
typedef std::lock_guard<std::recursive_mutex> AutoSyncLock;

static char *gszCurrSyncServer = NULL;
static int serverIdx = -1;

static tABC_CC ABC_SyncServerRot(tABC_Error *pError);
static tABC_CC ABC_SyncGetServer(const char *szRepoKey,
                                 char **pszServer,
                                 tABC_Error *pError);

#define ABC_SYNC_ROT(code, desc) \
    { \
        e = code; \
        if (0 > e) \
        { \
            ABC_CHECK_RET(ABC_SyncServerRot(pError)); \
            ABC_FREE_STR(szServer); \
            ABC_CHECK_RET(ABC_SyncGetServer(szRepoKey, &szServer, pError)); \
            e = code; \
        } \
        ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, desc); \
    }

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

/**
 * Copies a tABC_SyncKeys structure and all its contents.
 */
tABC_CC ABC_SyncKeysCopy(tABC_SyncKeys **ppOut,
                         tABC_SyncKeys *pIn,
                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoSyncKeys pKeys;

    ABC_NEW(pKeys.get(), tABC_SyncKeys);
    ABC_STRDUP(pKeys->szSyncDir, pIn->szSyncDir);
    ABC_STRDUP(pKeys->szSyncKey, pIn->szSyncKey);
    ABC_BUF_DUP(pKeys->MK, pIn->MK);

    *ppOut = pKeys;
    pKeys.get() = NULL;

exit:
    return cc;
}

/**
 * Frees a tABC_SyncKeys structure and all its members.
 */
void ABC_SyncFreeKeys(tABC_SyncKeys *pKeys)
{
    if (pKeys)
    {
        ABC_FREE_STR(pKeys->szSyncDir);
        ABC_FREE_STR(pKeys->szSyncKey);
        ABC_BUF_FREE(pKeys->MK);
        ABC_CLEAR_FREE(pKeys, sizeof(tABC_SyncKeys));
    }
}

/**
 * Initializes the underlying git library. Should be called at program start.
 */
tABC_CC ABC_SyncInit(const char *szCaCertPath, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e = 0;

    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "ABC_Sync has already been initalized");

    e = git_threads_init();
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "git_threads_init failed");
    gbInitialized = true;

    if (szCaCertPath)
    {
        e = git_libgit2_opts(GIT_OPT_SET_SSL_CERT_LOCATIONS, szCaCertPath, NULL);
        ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "git_libgit2_opts failed");
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
    if (gbInitialized)
    {
        git_threads_shutdown();
        gbInitialized = false;
    }
    ABC_FREE_STR(gszCurrSyncServer);
}

/**
 * Prepares a directory for syncing.
 * This will create the directory if it does not exist already.
 * Has no effect if the repo has already been created.
 */
tABC_CC ABC_SyncMakeRepo(const char *szRepoPath,
                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoSyncLock lock(gSyncMutex);
    int e = 0;

    git_repository_init_options opts = GIT_REPOSITORY_INIT_OPTIONS_INIT;
    git_repository *repo = NULL;

    opts.flags |= GIT_REPOSITORY_INIT_MKDIR;
    opts.flags |= GIT_REPOSITORY_INIT_MKPATH;
    e = git_repository_init_ext(&repo, szRepoPath, &opts);
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "git_repository_init failed");

exit:
    if (e < 0) SyncLogGitError(e);
    if (repo) git_repository_free(repo);

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
                     const char *szRepoKey,
                     int *pDirty,
                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoSyncLock lock(gSyncMutex);
    int e = 0;
    char *szServer = NULL;

    git_repository *repo = NULL;
    int dirty, need_push;

    ABC_CHECK_RET(ABC_SyncGetServer(szRepoKey, &szServer, pError));

    e = git_repository_open(&repo, szRepoPath);
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "git_repository_open failed");

    e = sync_fetch(repo, szServer);
    ABC_SYNC_ROT(sync_fetch(repo, szServer), "sync_fetch failed");

    {
        AutoCoreLock lock(gCoreMutex);
        e = sync_master(repo, &dirty, &need_push);
    }
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

    ABC_FREE_STR(szServer);

    return cc;
}

/**
 * Chooses a new server to use for syncing
 */
static
tABC_CC ABC_SyncServerRot(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoSyncLock lock(gSyncMutex);
    tABC_GeneralInfo *pInfo = NULL;

    ABC_CHECK_RET(ABC_GeneralGetInfo(&pInfo, pError));

    if (serverIdx == -1)
    {
        // Choose a random server to start with
        srand((unsigned) time(NULL));
        serverIdx = rand() % pInfo->countSyncServers;
    }
    else
    {
        serverIdx++;
    }
    if (serverIdx >= (int)pInfo->countSyncServers)
    {
        serverIdx = 0;
    }
    ABC_FREE_STR(gszCurrSyncServer);
    ABC_STRDUP(gszCurrSyncServer, pInfo->aszSyncServers[serverIdx]);
exit:
    ABC_GeneralFreeInfo(pInfo);

    return cc;
}

/**
 * Using the settings pick a repo and create the repo URI.
 *
 * @param szRepoKey    The repo key.
 * @param pszServer    Pointer to pointer where the resulting server URI
 *                     will be stored. Caller must free.
 */
static
tABC_CC ABC_SyncGetServer(const char *szRepoKey, char **pszServer, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoSyncLock lock(gSyncMutex);
    AutoU08Buf URL;

    ABC_CHECK_NULL(szRepoKey);

    if (!gszCurrSyncServer)
    {
        ABC_CHECK_RET(ABC_SyncServerRot(pError));
    }
    ABC_CHECK_ASSERT(gszCurrSyncServer != NULL,
        ABC_CC_SysError, "Unable to find a sync server");

    ABC_BUF_DUP_PTR(URL, gszCurrSyncServer, strlen(gszCurrSyncServer));

    // Do we have a trailing slash?
    if (URL.p[URL.end - URL.p - 1] != '/')
    {
        ABC_BUF_APPEND_PTR(URL, "/", 1);
    }
    ABC_BUF_APPEND_PTR(URL, szRepoKey, strlen(szRepoKey));
    ABC_BUF_APPEND_PTR(URL, "", 1);

    *pszServer = (char *)ABC_BUF_PTR(URL);
    ABC_BUF_CLEAR(URL);

    ABC_DebugLog("Syncing to: %s\n", *pszServer);

exit:
    return cc;
}

} // namespace abcd
