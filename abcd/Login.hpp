/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * An object representing a logged-in account.
 */

#ifndef ABC_Login_h
#define ABC_Login_h

#include "../src/ABC.h"
#include "util/Sync.hpp"

namespace abcd {

typedef struct sABC_Login
{
    // Identity:
    char            *szUserName;
    int             AccountNum;
    tABC_U08Buf     L1;

    // Account access:
    tABC_U08Buf     MK;
    char            *szSyncKey; // Hex-encoded
} tABC_Login;

// Destructor:
void ABC_LoginFree(tABC_Login *pSelf);

tABC_CC ABC_LoginNew(tABC_Login **ppSelf,
                     const char *szUserName,
                     tABC_Error *pError);

// Constructors:
tABC_CC ABC_LoginCreate(const char *szUserName,
                        const char *szPassword,
                        tABC_Login **ppSelf,
                        tABC_Error *pError);

// Read accessors:
tABC_CC ABC_LoginCheckUserName(tABC_Login *pSelf,
                               const char *szUserName,
                               int *pMatch,
                               tABC_Error *pError);

tABC_CC ABC_LoginGetSyncKeys(tABC_Login *pSelf,
                             tABC_SyncKeys **ppKeys,
                             tABC_Error *pError);

tABC_CC ABC_LoginGetServerKeys(tABC_Login *pSelf,
                               tABC_U08Buf *pL1,
                               tABC_U08Buf *pLP1,
                               tABC_Error *pError);

// Utility:
tABC_CC ABC_LoginFixUserName(const char *szUserName,
                             char **pszOut,
                             tABC_Error *pError);

} // namespace abcd

#endif
