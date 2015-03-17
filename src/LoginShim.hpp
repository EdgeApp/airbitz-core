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

#include "../abcd/util/Status.hpp"
#include "../abcd/util/Sync.hpp"
#include <memory>
#include <mutex>

namespace abcd {

class Lobby;
class Login;

/**
 * This mutex guards the cached login objects.
 */
extern std::mutex gLoginMutex;
typedef std::lock_guard<std::mutex> AutoLoginLock;

extern std::shared_ptr<Lobby> gLobbyCache;
extern std::shared_ptr<Login> gLoginCache;

/**
 * Clears all cached login objects.
 */
void
cacheLogout();

/**
 * Loads the lobby for the given user into the cache.
 * The caller should already be holding the login mutex,
 * and must continue holding the mutex while accessing the object.
 */
Status
cacheLobby(const char *szUserName);

/**
 * Creates a new account and adds it to the cache.
 * The caller should already be holding the login mutex,
 * and must continue holding the mutex while accessing the object.
 */
Status
cacheLoginNew(const char *szUserName, const char *szPassword);

/**
 * Logs the user in with a password, if necessary.
 * The caller should already be holding the login mutex,
 * and must continue holding the mutex while accessing the object.
 */
Status
cacheLoginPassword(const char *szUserName, const char *szPassword);

/**
 * Logs the user in with their recovery answers, if necessary.
 * The caller should already be holding the login mutex,
 * and must continue holding the mutex while accessing the object.
 */
Status
cacheLoginRecovery(const char *szUserName, const char *szRecoveryAnswers);

/**
 * Logs the user in with their PIN, if necessary.
 * The caller should already be holding the login mutex,
 * and must continue holding the mutex while accessing the object.
 */
Status
cacheLoginPin(const char *szUserName, const char *szPin);

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
