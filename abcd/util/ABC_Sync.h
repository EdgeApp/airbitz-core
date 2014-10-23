/**
 * @file
 * AirBitz file-sync functions prototypes.
 *
 * See LICENSE for copy, modification, and use permissions
 *
 * @author See AUTHORS
 * @version 1.0
 */

#ifndef ABC_Sync_h
#define ABC_Sync_h

#include "ABC.h"
#include "ABC_Util.h"

#define SYNC_KEY_LENGTH 20

#ifdef __cplusplus
extern "C" {
#endif

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

    void ABC_SyncFreeKeys(tABC_SyncKeys *pKeys);

    tABC_CC ABC_SyncInit(const char *szCaCertPath, tABC_Error *pError);

    void ABC_SyncTerminate();

    tABC_CC ABC_SyncMakeRepo(const char *szRepoPath,
                             tABC_Error *pError);

    tABC_CC ABC_SyncRepo(const char *szRepoPath,
                         const char *szRepoKey,
                         int *pDirty,
                         tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
