/**
 * @file
 * AirBitz file-sync functions.
 */

#include "ABC_Sync.h"
#include "ABC_Util.h"
#include "sync.h"

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
 * Initializes the underlying git library. Should be called at program start.
 */
tABC_CC ABC_SyncInit(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    e = git_threads_init();
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "git_threads_init failed");

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

    e = git_repository_init(&repo, szRepoPath, 0);
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
                     const char *szServer,
                     int *pDirty,
                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    git_repository *repo = NULL;
    int dirty, need_push;

    e = git_repository_open(&repo, szRepoPath);
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "git_repository_open failed");

    e = sync_fetch(repo, szServer);
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "sync_fetch failed");

    e = sync_master(repo, &dirty, &need_push);
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

    return cc;
}
