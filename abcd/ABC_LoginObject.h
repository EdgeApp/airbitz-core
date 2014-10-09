/**
 * @file
 * An object representing a logged-in account.
 */

#ifndef ABC_LoginObject_h
#define ABC_LoginObject_h

#include "ABC.h"
#include "util/ABC_Sync.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct sABC_LoginObject tABC_LoginObject;

    // Destructor:
    void ABC_LoginObjectFree(tABC_LoginObject *pSelf);

    // Constructors:
    tABC_CC ABC_LoginObjectCreate(const char *szUserName,
                                  const char *szPassword,
                                  tABC_LoginObject **ppSelf,
                                  tABC_Error *pError);

    tABC_CC ABC_LoginObjectFromPassword(const char *szUserName,
                                        const char *szPassword,
                                        tABC_LoginObject **ppSelf,
                                        tABC_Error *pError);

    tABC_CC ABC_LoginObjectFromRecovery(const char *szUserName,
                                        const char *szRecoveryAnswers,
                                        tABC_LoginObject **ppSelf,
                                        tABC_Error *pError);

    // Actions:
    tABC_CC ABC_LoginObjectSync(tABC_LoginObject *pSelf,
                                int *pDirty,
                                tABC_Error *pError);

    // Write accessors:
    tABC_CC ABC_LoginObjectSetPassword(tABC_LoginObject *pSelf,
                                       const char *szPassword,
                                       tABC_Error *pError);

    tABC_CC ABC_LoginObjectSetRecovery(tABC_LoginObject *pSelf,
                                       const char *szRecoveryQuestions,
                                       const char *szRecoveryAnswers,
                                       tABC_Error *pError);

    // Read accessors:
    tABC_CC ABC_LoginObjectCheckUserName(tABC_LoginObject *pSelf,
                                         const char *szUserName,
                                         int *pMatch,
                                         tABC_Error *pError);

    tABC_CC ABC_LoginObjectGetSyncKeys(tABC_LoginObject *pSelf,
                                       tABC_SyncKeys **ppKeys,
                                       tABC_Error *pError);

    tABC_CC ABC_LoginObjectGetServerKeys(tABC_LoginObject *pSelf,
                                         tABC_U08Buf *pL1,
                                         tABC_U08Buf *pLP1,
                                         tABC_Error *pError);

    // Helper:
    tABC_CC ABC_LoginObjectGetRQ(const char *szUserName,
                                 char **pszRecoveryQuestions,
                                 tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
