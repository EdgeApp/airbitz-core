#include <ABC.h>
#include <ABC_Sync.h>
#include <ABC_Util.h>
#include <errno.h>
#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static tABC_CC RecreateDir(const char *szPath, tABC_Error *pError);

/**
 * Performs a test sync between two repositories.
 */
tABC_CC TestSync(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_SyncInit(pError));

    ABC_CHECK_RET(RecreateDir("sync_repo", pError));
    ABC_CHECK_RET(ABC_SyncMakeRepo("sync_repo", pError));
    FILE *foo = fopen("sync_repo""/foo.txt", "wb");
    fclose(foo);

    ABC_CHECK_RET(RecreateDir("server.git", pError));
    system("git init --bare server.git");

    ABC_CHECK_RET(ABC_SyncInitialPush("sync_repo", "server.git", pError));
    ABC_CHECK_RET(ABC_SyncInitialPush("sync_repo", "server.git", pError));

    ABC_CHECK_RET(RecreateDir("download_repo", pError));
    ABC_CHECK_RET(ABC_SyncMakeRepo("download_repo", pError));

    ABC_CHECK_RET(ABC_SyncRepo("download_repo", "server.git", pError));

    ABC_SyncTerminate();

exit:
    return cc;
}

int main()
{
    tABC_CC cc;
    tABC_Error error;

    cc = TestSync(&error);
    if (cc != ABC_CC_Ok)
    {
        fprintf(stderr, "%s:%d: %s returned error %d (%s)\n",
                error.szSourceFile, error.nSourceLine,
                error.szSourceFunc, cc, error.szDescription);
        return 1;
    }

    return 0;
}

/**
 * Callback for recusive file deletion.
 */
static int DeleteCallback(const char *fpath, const struct stat *sb,
                       int typeflag, struct FTW *ftwbuf)
{
    return remove(fpath);
}

/**
 * Deletes and re-creates a directory.
 */
static tABC_CC RecreateDir(const char *szPath, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    struct stat s;
    e = stat(szPath, &s);
    if (!e)
    {
        ABC_CHECK_ASSERT(S_ISDIR(s.st_mode), ABC_CC_Error, "not a directory");

        e = nftw(szPath, DeleteCallback, 32, FTW_DEPTH | FTW_PHYS);
        ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "cannot delete directory");
    }
    else
    {
        ABC_CHECK_ASSERT(ENOENT == errno, ABC_CC_SysError, "cannot stat directory");
    }

    e = mkdir(szPath, S_IRWXU | S_IRWXG | S_IRWXO);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "mkdir failed");

exit:
    return cc;
}
