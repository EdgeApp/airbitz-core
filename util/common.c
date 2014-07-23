/**
 * @file
 * Common utility functions for the command-line wallet interface
 */
#include "common.h"
#include <stdio.h>

void PrintError(tABC_CC cc, tABC_Error *pError)
{
    if (cc != ABC_CC_Ok)
    {
        fprintf(stderr, "%s:%d: %s returned error %d (%s)\n",
                pError->szSourceFile, pError->nSourceLine,
                pError->szSourceFunc, cc, pError->szDescription);
    }
}
