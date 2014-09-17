#include <ABC.h>
#include <ABC_FileIO.h>
#include <ABC_Sync.h>
#include <ABC_Util.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static tABC_CC TestRecreateDir(const char *szPath, tABC_Error *pError);
static tABC_CC TestCreateFile(const char *szPath, const char *szContents, tABC_Error *pError);

#define check(f) \
    ABC_CHECK_ASSERT(0 <= f, ABC_CC_SysError, strerror(errno))

/**
 * Performs a test sync between two repositories.
 */
tABC_CC TestSync(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int dirty;

    ABC_CHECK_RET(ABC_SyncInit("../util", pError));

    ABC_CHECK_RET(TestRecreateDir("server.git", pError));
    ABC_CHECK_RET(TestRecreateDir("sync_a", pError));
    ABC_CHECK_RET(TestRecreateDir("sync_b", pError));

    check(system("git init --bare server.git"));
    ABC_CHECK_RET(ABC_SyncMakeRepo("sync_a", pError));
    ABC_CHECK_RET(ABC_SyncMakeRepo("sync_b", pError));

    // Start repo a:
    ABC_CHECK_RET(TestCreateFile("sync_a/a.txt", "a\n", pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_a", "server.git", &dirty, pError));

    // Start repo b:
    ABC_CHECK_RET(TestCreateFile("sync_b/b.txt", "b\n", pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_b", "server.git", &dirty, pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_a", "server.git", &dirty, pError));

    // Create a conflict:
    check(remove("sync_a/a.txt"));
    ABC_CHECK_RET(TestCreateFile("sync_a/c.txt", "a\n", pError));
    ABC_CHECK_RET(TestCreateFile("sync_b/c.txt", "b\n", pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_a", "server.git", &dirty, pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_b", "server.git", &dirty, pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_a", "server.git", &dirty, pError));

    // Create a subdir:
    check(mkdir("sync_a/sub", S_IRWXU | S_IRWXG | S_IRWXO));
    ABC_CHECK_RET(TestCreateFile("sync_a/sub/a.txt", "a\n", pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_a", "server.git", &dirty, pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_b", "server.git", &dirty, pError));

    // Subdir chaos:
    ABC_CHECK_RET(TestCreateFile("sync_b/sub/b.txt", "a\n", pError));
    check(remove("sync_a/sub/a.txt"));
    ABC_CHECK_RET(TestCreateFile("sync_a/sub/c.txt", "a\n", pError));
    ABC_CHECK_RET(TestCreateFile("sync_b/sub/c.txt", "b\n", pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_a", "server.git", &dirty, pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_b", "server.git", &dirty, pError));
    ABC_CHECK_RET(ABC_SyncRepo("sync_a", "server.git", &dirty, pError));

    // When this is done, the two subdirs should match exactly.

exit:
    ABC_SyncTerminate();

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
 * Deletes and re-creates a directory.
 */
static tABC_CC TestRecreateDir(const char *szPath, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    ABC_CHECK_RET(ABC_FileIODeleteRecursive(szPath, pError));

    e = mkdir(szPath, S_IRWXU | S_IRWXG | S_IRWXO);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "mkdir failed");

exit:
    return cc;
}

/**
 * Deletes and re-creates a directory.
 */
static tABC_CC TestCreateFile(const char *szPath, const char *szContents, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    FILE *file = NULL;

    file = fopen(szPath, "w");
    ABC_CHECK_ASSERT(file, ABC_CC_SysError, "cannot create file");
    e = fwrite(szContents, strlen(szContents), 1, file);
    ABC_CHECK_ASSERT(0 <= e, ABC_CC_SysError, "cannot create file");

exit:
    if (file)       fclose(file);

    return cc;
}
