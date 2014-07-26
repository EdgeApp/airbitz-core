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

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_SyncInit(tABC_Error *pError);

    void ABC_SyncTerminate();

    tABC_CC ABC_SyncMakeRepo(const char *szRepoPath,
                             tABC_Error *pError);

    tABC_CC ABC_SyncRepo(const char *szRepoPath,
                         const char *szServer,
                         int *pDirty,
                         tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
