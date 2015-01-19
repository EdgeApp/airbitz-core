/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * AirBitz Login functions.
 *
 * This file wrapps the methods of `LoginObject.hpp` with a caching layer
 * for backwards-compatibility with the old API.
 */

#ifndef ABC_LoginShim_h
#define ABC_LoginShim_h

#include "ABC.h"
#include "../abcd/util/Sync.hpp"
#include <time.h>

namespace abcd {

void ABC_LoginShimLogout();

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

tABC_CC ABC_LoginShimPinLogin(const char *szUserName,
                              const char *szPin,
                              tABC_Error *pError);

tABC_CC ABC_LoginShimPinSetup(const char *szUserName,
                              const char *szPassword,
                              const char *szPin,
                              time_t expires,
                              tABC_Error *pError);

tABC_CC ABC_LoginShim2FAEnable(const char *szUserName,
                               const char *szPassword,
                               long timeout,
                               tABC_Error *pError);

tABC_CC ABC_LoginShim2FADisable(const char *szUserName,
                                const char *szPassword,
                                tABC_Error *pError);

tABC_CC ABC_LoginShim2FAStatus(const char *szUserName, const char *szPassword,
        bool *on, long *timeout, tABC_Error *pError);

tABC_CC ABC_LoginShim2FAGetSecret(const char *szUserName,
                                  const char *szPassword,
                                  char **pszSecret,
                                  tABC_Error *pError);

tABC_CC ABC_LoginShim2FAQrCode(const char *szUserName, const char *szPassword,
    unsigned char **paData, unsigned int *pWidth, tABC_Error *pError);

tABC_CC ABC_LoginShim2FASetSecret(const char *szUserName,
                                  const char *szPassword,
                                  const char *szSecret,
                                  bool loggedIn,
                                  tABC_Error *pError);

tABC_CC ABC_LoginShim2FARequestReset(const char *szUserName,
                                     const char *szPassword,
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

tABC_CC ABC_LoginShimPasswordOk(const char *szUserName,
                                const char *szPassword,
                                bool *pOk,
                                tABC_Error *pError);

} // namespace abcd

#endif
