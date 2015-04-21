/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Functions for communicating with the AirBitz login servers.
 */

#ifndef ABCD_LOGIN_LOGIN_SERVER_HPP
#define ABCD_LOGIN_LOGIN_SERVER_HPP

#include "LoginPackages.hpp"
#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <time.h>
#include <list>

namespace abcd {

class Login;

// We need a better way to get this data out than writing to globals:
extern std::string gOtpResetDate;

tABC_CC ABC_LoginServerCreate(tABC_U08Buf L1,
                              tABC_U08Buf LP1,
                              const CarePackage &carePackage,
                              const LoginPackage &loginPackage,
                              const char *szRepoAcctKey,
                              tABC_Error *pError);

tABC_CC ABC_LoginServerActivate(tABC_U08Buf L1,
                                tABC_U08Buf LP1,
                                tABC_Error *pError);

tABC_CC ABC_LoginServerAvailable(tABC_U08Buf L1,
                                 tABC_Error *pError);

tABC_CC ABC_LoginServerChangePassword(tABC_U08Buf L1,
                                      tABC_U08Buf oldLP1,
                                      tABC_U08Buf newLP1,
                                      tABC_U08Buf newLRA1,
                                      const CarePackage &carePackage,
                                      const LoginPackage &loginPackage,
                                      tABC_Error *pError);

tABC_CC ABC_LoginServerGetCarePackage(tABC_U08Buf L1,
                                      CarePackage &result,
                                      tABC_Error *pError);

tABC_CC ABC_LoginServerGetLoginPackage(tABC_U08Buf L1,
                                       tABC_U08Buf LP1,
                                       tABC_U08Buf LRA1,
                                       LoginPackage &result,
                                       tABC_Error *pError);

tABC_CC ABC_LoginServerGetPinPackage(tABC_U08Buf DID,
                                     tABC_U08Buf LPIN1,
                                     char **szPinPackage,
                                     tABC_Error *pError);

tABC_CC ABC_LoginServerUpdatePinPackage(tABC_U08Buf L1,
                                        tABC_U08Buf LP1,
                                        tABC_U08Buf DID,
                                        tABC_U08Buf LPIN1,
                                        const std::string &pinPackage,
                                        time_t ali,
                                        tABC_Error *pError);
/**
 * Create a git repository on the server, suitable for holding a wallet.
 */
Status
LoginServerWalletCreate(tABC_U08Buf L1, tABC_U08Buf LP1, const char *syncKey);

/**
 * Lock the server wallet repository, so it is not automatically deleted.
 */
Status
LoginServerWalletActivate(tABC_U08Buf L1, tABC_U08Buf LP1, const char *syncKey);

tABC_CC ABC_LoginServerOtpEnable(tABC_U08Buf L1, tABC_U08Buf LP1, const char *szOtpToken, const long timeout, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpDisable(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpStatus(tABC_U08Buf L1, tABC_U08Buf LP1, bool *on, long *timeout, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpReset(tABC_U08Buf L1, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpPending(std::list<DataChunk> users, std::list<bool> &isPending, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpResetCancelPending(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_Error *pError);

tABC_CC ABC_LoginServerUploadLogs(tABC_U08Buf L1,
                                  tABC_U08Buf LP1,
                                  const Login &login,
                                  tABC_Error *pError);

} // namespace abcd

#endif
