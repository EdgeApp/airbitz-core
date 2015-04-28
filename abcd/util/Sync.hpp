/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Wrappers around the AirBitz git-sync library.
 */

#ifndef ABC_Sync_h
#define ABC_Sync_h

#include "../../src/ABC.h"

#define SYNC_KEY_LENGTH 20

namespace abcd {

tABC_CC ABC_SyncInit(const char *szCaCertPath, tABC_Error *pError);

void ABC_SyncTerminate();

tABC_CC ABC_SyncMakeRepo(const char *szRepoPath,
                         tABC_Error *pError);

tABC_CC ABC_SyncRepo(const char *szRepoPath,
                     const char *szRepoKey,
                     bool &dirty,
                     tABC_Error *pError);

} // namespace abcd

#endif
