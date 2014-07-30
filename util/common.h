/**
 * @file
 * Common utility functions for the command-line wallet interface
 */

#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#include <ABC.h>

#define MAIN_CHECK(f) \
    if (ABC_CC_Ok != (cc = f))\
    { \
        PrintError(cc, &error); \
        return 1; \
    }

#define CA_CERT "./ca-certificates.crt"

/**
 * Examines a return code and prints an error if it is not OK.
 */
void PrintError(tABC_CC cc, tABC_Error *pError);

#endif
