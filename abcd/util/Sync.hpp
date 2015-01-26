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
#include "U08Buf.hpp"

#define SYNC_KEY_LENGTH 20

namespace abcd {

/**
 * Contains everything needed to access a sync repo.
 */
typedef struct sABC_SyncKeys
{
    /** The directory that contains the synced files: */
    char *szSyncDir;
    /** The sync key used to access the server: */
    char *szSyncKey;
    /** The encryption key used to protect the contents: */
    tABC_U08Buf MK;
} tABC_SyncKeys;

tABC_CC ABC_SyncKeysCopy(tABC_SyncKeys **ppOut,
                         tABC_SyncKeys *pIn,
                         tABC_Error *pError);

void ABC_SyncFreeKeys(tABC_SyncKeys *pKeys);

tABC_CC ABC_SyncInit(const char *szCaCertPath, tABC_Error *pError);

void ABC_SyncTerminate();

tABC_CC ABC_SyncMakeRepo(const char *szRepoPath,
                         tABC_Error *pError);

tABC_CC ABC_SyncRepo(const char *szRepoPath,
                     const char *szRepoKey,
                     int *pDirty,
                     tABC_Error *pError);

} // namespace abcd

#endif
