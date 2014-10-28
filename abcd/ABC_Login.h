/**
 * @file
 * An object representing a logged-in account.
 */

#ifndef ABC_Login_h
#define ABC_Login_h

#include "ABC.h"
#include "util/ABC_Sync.h"

#ifdef __cplusplus
extern "C" {
#endif

    typedef struct sABC_Login tABC_Login;

    // Destructor:
    void ABC_LoginFree(tABC_Login *pSelf);

    // Constructors:
    tABC_CC ABC_LoginCreate(const char *szUserName,
                            const char *szPassword,
                            tABC_Login **ppSelf,
                            tABC_Error *pError);

    tABC_CC ABC_LoginFromPassword(const char *szUserName,
                                  const char *szPassword,
                                  tABC_Login **ppSelf,
                                  tABC_Error *pError);

    tABC_CC ABC_LoginFromRecovery(const char *szUserName,
                                  const char *szRecoveryAnswers,
                                  tABC_Login **ppSelf,
                                  tABC_Error *pError);

    // Write accessors:
    tABC_CC ABC_LoginSetPassword(tABC_Login *pSelf,
                                 const char *szPassword,
                                 tABC_Error *pError);

    tABC_CC ABC_LoginSetRecovery(tABC_Login *pSelf,
                                 const char *szRecoveryQuestions,
                                 const char *szRecoveryAnswers,
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

    // Helper:
    tABC_CC ABC_LoginGetRQ(const char *szUserName,
                           char **pszRecoveryQuestions,
                           tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
