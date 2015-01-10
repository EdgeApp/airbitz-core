/**
 * @file
 * Common utility functions for the command-line wallet interface
 */
#include "common.h"
#include <stdio.h>
#include <stdlib.h>

void PrintError(tABC_CC cc, tABC_Error *pError)
{
    if (cc != ABC_CC_Ok)
    {
        fprintf(stderr, "%s:%d: %s returned error %d (%s)\n",
                pError->szSourceFile, pError->nSourceLine,
                pError->szSourceFunc, cc, pError->szDescription);
    }
}

char *Slurp(const char *szFilename)
{
    // Where's the error checking?
    FILE *f;
    long l;
    char *buffer;

    f = fopen(szFilename, "r");
    fseek(f , 0L , SEEK_END);
    l = ftell(f);
    rewind(f);

    buffer = (char*)malloc(l + 1);
    if (!buffer) {
        return NULL;
    }
    if (fread(buffer, l, 1, f) != 1) {
        return NULL;
    }

    fclose(f);
    return buffer;
}

