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
#include "../abcd/util/Status.hpp"
#include "../abcd/util/Sync.hpp"
#include <time.h>
#include <mutex>

namespace abcd {

class Lobby;
class Login;

/**
 * This mutex guards the cached login objects.
 */
extern std::mutex gLoginMutex;
typedef std::lock_guard<std::mutex> AutoLoginLock;

extern Lobby *gLobbyCache;
extern Login *gLoginCache;

/**
 * Loads the lobby for the given user into the cache.
 * The caller should already be holding the login mutex,
 * and must continue holding the mutex while accessing the object.
 */
Status
cacheLobby(const char *szUserName);

/**
 * Loads the account for the given user into the cache.
 * The caller should already be holding the login mutex,
 * and must continue holding the mutex while accessing the object.
 */
Status
cacheLogin(const char *szUserName, const char *szPassword);

/**
 * Clears all cached login objects.
 */
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

tABC_CC ABC_LoginShimGetSyncKeys(const char *szUserName,
                                 const char *szPassword,
                                 tABC_SyncKeys **ppKeys,
                                 tABC_Error *pError);

tABC_CC ABC_LoginShimGetServerKeys(const char *szUserName,
                                   const char *szPassword,
                                   tABC_U08Buf *pL1,
                                   tABC_U08Buf *pLP1,
                                   tABC_Error *pError);

} // namespace abcd

#endif
