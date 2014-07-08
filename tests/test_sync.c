#include <ABC.h>
#include <ABC_Util.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

tABC_CC TestSync(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    e = mkdir("sync_repo", S_IRWXU | S_IRWXG | S_IRWXO);
    ABC_CHECK_ASSERT(!e, ABC_CC_SysError, "mkdir failed");

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
