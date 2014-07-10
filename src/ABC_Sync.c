/**
 * @file
 * AirBitz file-sync functions.
 */

#include <git2.h>
#include "ABC_Sync.h"

#define SYNC_REFSPEC                    "+refs/heads/*:refs/remotes/sync/*"
#define SYNC_GIT_NAME                   "airbitz"
#define SYNC_GIT_EMAIL                  ""

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
 * Helper function to perform the fetch
 */
static tABC_CC SyncFetch(git_repository *repo,
                         const char *szServer,
                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    git_remote *remote = NULL;
    git_signature *sig = NULL;

    e = git_remote_create_anonymous(&remote, repo, szServer, SYNC_REFSPEC);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_remote_create_anonymous failed");

    e = git_signature_now(&sig, SYNC_GIT_NAME, SYNC_GIT_EMAIL);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_signature_now failed");

    e = git_remote_fetch(remote, sig, "sync fetch");
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_remote_fetch failed");

exit:
    if (e)          SyncLogGitError(e);
    if (remote)     git_remote_free(remote);
    if (sig)        git_signature_free(sig);

    return cc;
}

/**
 * Helper function to perform add and commit
 */
static tABC_CC SyncCommit(git_repository *repo,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    git_index *index = NULL;
    git_tree *tree = NULL;
    git_signature *sig = NULL;
    git_oid tree_id, commit_id;

    e = git_repository_index(&index, repo);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_repository_index failed");

    char *strs[] = {"*"};
    git_strarray paths = {strs, 1};

    e = git_index_add_all(index, &paths, 0, NULL, NULL);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_index_add_all failed");

    e = git_index_write_tree(&tree_id, index);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_index_write_tree failed");

    e = git_tree_lookup(&tree, repo, &tree_id);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_tree_lookup failed");

    e = git_signature_now(&sig, SYNC_GIT_NAME, SYNC_GIT_EMAIL);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_signature_now failed");

    git_oid parent;
    int count = 1;
    e = git_reference_name_to_id(&parent, repo, "HEAD");
    if (e == GIT_ENOTFOUND)
    {
        count = 0;
    }
    else
    {
        ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_reference_name_to_id failed");
    }

    char message[256];
    sprintf(message, "%s - %ld", "Adding client generated files", time(NULL));
    e = git_commit_create_v(&commit_id, repo, "HEAD", sig, sig, NULL, message, tree, count, &parent);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_commit_create_v failed");

    e = git_index_write(index);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_index_write failed");

exit:
    if (e)          SyncLogGitError(e);
    if (index)      git_index_free(index);
    if (tree)       git_tree_free(tree);
    if (sig)        git_signature_free(sig);

    return cc;
}

/**
 * Helper function to perform push.
 */
static tABC_CC SyncPush(git_repository *repo,
                        const char *szServer,
                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    git_remote *remote = NULL;
    git_push *push = NULL;

    e = git_remote_create_anonymous(&remote, repo, szServer, SYNC_REFSPEC);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_remote_create_anonymous failed");

    e = git_remote_connect(remote, GIT_DIRECTION_PUSH);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_remote_connect failed");

    e = git_push_new(&push, remote);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_push_new failed");

    e = git_push_add_refspec(push, "refs/heads/master");
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_push_add_refspec failed");

    e = git_push_finish(push);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_push_finish failed");

    ABC_CHECK_ASSERT(git_push_unpack_ok(push), ABC_CC_SysError, "git_push_unpack_ok failed");

exit:
    if (e)          SyncLogGitError(e);
    if (remote)     git_remote_free(remote);
    if (push)       git_push_free(push);

    return cc;
}

/**
 * Initializes the underlying git library. Should be called at program start.
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
 * Pushes a repository to the server for the first time. This should only
 * be done once when the account or wallet are created for the first time.
 * The normal sync operation will not work until this has been done.
 */
tABC_CC ABC_SyncInitialPush(const char *szRepoPath,
                            const char *szServer,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    git_repository *repo = NULL;

    e = git_repository_open(&repo, szRepoPath);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_repository_open failed");

    ABC_CHECK_RET(SyncCommit(repo, pError));
    ABC_CHECK_RET(SyncPush(repo, szServer, pError));

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
                     const char *szServer,
                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    git_repository *repo = NULL;

    e = git_repository_open(&repo, szRepoPath);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "git_repository_open failed");

    ABC_CHECK_RET(SyncFetch(repo, szServer, pError));
    // Commit, merge, and push...

exit:
    if (repo) git_repository_free(repo);

    return cc;
}
