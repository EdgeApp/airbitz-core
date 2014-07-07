/**
 * @file
 * AirBitz file-sync functions.
 */

#include <git2.h>
#include "ABC_Sync.h"

/**
 * Initializes the underlying git library. Should be called a program start.
 */
tABC_CC ABC_SyncInit(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    e = git_threads_init();
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_threads_init failed");

exit:
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
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_repository_init failed");

exit:
    if (repo) git_repository_free(repo);

    return cc;
}

/**
 * Synchronizes the directory with the server. New files in the folder will
 * go up to the server, and new files on the server will come down to the
 * directory. If there is a conflict, the newer file will win.
 */
tABC_CC ABC_SyncRepo(const char *szRepoPath,
                     const char *szRepoKey,
                     const char *szServer,
                     tABC_Error *pError)
{
    return ABC_CC_Ok;
}
