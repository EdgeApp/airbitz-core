/**
 * @file
 * AirBitz file-sync functions prototypes.
 */

#ifndef ABC_Sync_h
#define ABC_Sync_h

#include "ABC.h"
#include "ABC_Util.h"

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_SyncInit(tABC_Error *pError);

    void ABC_SyncTerminate();

    tABC_CC ABC_SyncMakeRepo(const char *szRepoPath,
                             tABC_Error *pError);

    tABC_CC ABC_SyncInitialPush(const char *szRepoPath,
                                const char *szRepoKey,
                                const char *szServer,
                                tABC_Error *pError);

    tABC_CC ABC_SyncRepo(const char *szRepoPath,
                         const char *szRepoKey,
                         const char *szServer,
                         tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
