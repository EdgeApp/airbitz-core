/**
 * @file
 * AirBitz Login functions.
 *
 * This file wrapps the methods of `ABC_LoginObject.h` with a caching layer
 * for backwards-compatibility with the old API.
 */

#ifndef ABC_LoginShim_h
#define ABC_LoginShim_h

#include "ABC.h"
#include "util/ABC_Sync.h"

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_LoginShimLogout(tABC_Error *pError);

    // Blocking functions (see ABC_LoginRequest):
    tABC_CC ABC_LoginShimLogin(const char *szUserName,
                               const char *szPassword,
                               tABC_Error *pError);

    tABC_CC ABC_LoginShimNewAccount(const char *szUserName,
                                    const char *szPassword,
                                    tABC_Error *pError);

    tABC_CC ABC_LoginShimSetRecovery(const char *szUserName,
                                     const char *szPassword,
                                     const char *szRecoveryQuestions,
                                     const char *szRecoveryAnswers,
                                     tABC_Error *pError);

    tABC_CC ABC_LoginShimSetPassword(const char *szUserName,
                                     const char *szPassword,
                                     const char *szRecoveryAnswers,
                                     const char *szNewPassword,
                                     tABC_Error *pError);

    // Ordinary functions:
    tABC_CC ABC_LoginShimCheckRecovery(const char *szUserName,
                                       const char *szRecoveryAnswers,
                                       bool *pbValid,
                                       tABC_Error *pError);

    tABC_CC ABC_LoginShimGetRecovery(const char *szUserName,
                                     char **pszQuestions,
                                     tABC_Error *pError);

    tABC_CC ABC_LoginShimGetSyncKeys(const char *szUserName,
                                     const char *szPassword,
                                     tABC_SyncKeys **ppKeys,
                                     tABC_Error *pError);

    tABC_CC ABC_LoginShimGetServerKeys(const char *szUserName,
                                       const char *szPassword,
                                       tABC_U08Buf *pL1,
                                       tABC_U08Buf *pLP1,
                                       tABC_Error *pError);

    tABC_CC ABC_LoginShimCheckPasswordChange(const char *szUserName,
                                             const char *szPassword,
                                             tABC_Error *pError);

    tABC_CC ABC_LoginShimSync(const char *szUserName,
                              const char *szPassword,
                              int *pDirty,
                              tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
