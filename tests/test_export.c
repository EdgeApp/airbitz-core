#include <ABC.h>
#include <util/ABC_FileIO.h>
#include <util/ABC_Sync.h>
#include <util/ABC_Util.h>
#include <csv.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>


/**
 * Performs a test sync between two repositories.
 */
tABC_CC TestExport(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

exit:
    return cc;
}

int main()
{
    tABC_CC cc;
    tABC_Error error;

    cc = TestExport(&error);
    if (cc != ABC_CC_Ok)
    {
        fprintf(stderr, "%s:%d: %s returned error %d (%s)\n",
                error.szSourceFile, error.nSourceLine,
                error.szSourceFunc, cc, error.szDescription);
        return 1;
    }

    return 0;
}

