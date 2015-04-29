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

class Lobby;
class Login;
class Account;

// We need a better way to get this data out than writing to globals:
extern std::string gOtpResetDate;

Status
loginServerGetGeneral(JsonPtr &result);

Status
loginServerGetQuestions(JsonPtr &result);

tABC_CC ABC_LoginServerCreate(const Lobby &lobby,
                              tABC_U08Buf LP1,
                              const CarePackage &carePackage,
                              const LoginPackage &loginPackage,
                              const char *szRepoAcctKey,
                              tABC_Error *pError);

tABC_CC ABC_LoginServerActivate(const Lobby &lobby,
                                tABC_U08Buf LP1,
                                tABC_Error *pError);

tABC_CC ABC_LoginServerAvailable(const Lobby &lobby,
                                 tABC_Error *pError);

tABC_CC ABC_LoginServerChangePassword(const Lobby &lobby,
                                      tABC_U08Buf oldLP1,
                                      tABC_U08Buf newLP1,
                                      tABC_U08Buf newLRA1,
                                      const CarePackage &carePackage,
                                      const LoginPackage &loginPackage,
                                      tABC_Error *pError);

tABC_CC ABC_LoginServerGetCarePackage(const Lobby &lobby,
                                      CarePackage &result,
                                      tABC_Error *pError);

tABC_CC ABC_LoginServerGetLoginPackage(const Lobby &lobby,
                                       tABC_U08Buf LP1,
                                       tABC_U08Buf LRA1,
                                       LoginPackage &result,
                                       tABC_Error *pError);

tABC_CC ABC_LoginServerGetPinPackage(tABC_U08Buf DID,
                                     tABC_U08Buf LPIN1,
                                     char **szPinPackage,
                                     tABC_Error *pError);

tABC_CC ABC_LoginServerUpdatePinPackage(const Lobby &lobby,
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
LoginServerWalletCreate(const Lobby &lobby, tABC_U08Buf LP1, const char *syncKey);

/**
 * Lock the server wallet repository, so it is not automatically deleted.
 */
Status
LoginServerWalletActivate(const Lobby &lobby, tABC_U08Buf LP1, const char *syncKey);

tABC_CC ABC_LoginServerOtpEnable(const Lobby &lobby, tABC_U08Buf LP1, const char *szOtpToken, const long timeout, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpDisable(const Lobby &lobby, tABC_U08Buf LP1, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpStatus(const Lobby &lobby, tABC_U08Buf LP1, bool *on, long *timeout, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpReset(const Lobby &lobby, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpPending(std::list<DataChunk> users, std::list<bool> &isPending, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpResetCancelPending(const Lobby &lobby, tABC_U08Buf LP1, tABC_Error *pError);

tABC_CC ABC_LoginServerUploadLogs(const Account &account,
                                  tABC_Error *pError);

} // namespace abcd

#endif
