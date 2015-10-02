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

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <time.h>
#include <list>

namespace abcd {

class Account;
class Lobby;
class Login;
class JsonPtr;
struct CarePackage;
struct LoginPackage;

// We need a better way to get this data out than writing to globals:
extern std::string gOtpResetDate;

Status
loginServerGetGeneral(JsonPtr &result);

Status
loginServerGetQuestions(JsonPtr &result);

tABC_CC ABC_LoginServerCreate(const Lobby &lobby,
                              DataSlice LP1,
                              const CarePackage &carePackage,
                              const LoginPackage &loginPackage,
                              const std::string &syncKey,
                              tABC_Error *pError);

tABC_CC ABC_LoginServerActivate(const Login &login,
                                tABC_Error *pError);

tABC_CC ABC_LoginServerAvailable(const Lobby &lobby,
                                 tABC_Error *pError);

tABC_CC ABC_LoginServerChangePassword(const Login &login,
                                      DataSlice newLP1,
                                      DataSlice newLRA1,
                                      const CarePackage &carePackage,
                                      const LoginPackage &loginPackage,
                                      tABC_Error *pError);

tABC_CC ABC_LoginServerGetCarePackage(const Lobby &lobby,
                                      CarePackage &result,
                                      tABC_Error *pError);

tABC_CC ABC_LoginServerGetLoginPackage(const Lobby &lobby,
                                       DataSlice LP1,
                                       DataSlice LRA1,
                                       LoginPackage &result,
                                       tABC_Error *pError);

tABC_CC ABC_LoginServerGetPinPackage(DataSlice DID,
                                     DataSlice LPIN1,
                                     std::string &result,
                                     tABC_Error *pError);

tABC_CC ABC_LoginServerUpdatePinPackage(const Login &login,
                                        DataSlice DID,
                                        DataSlice LPIN1,
                                        const std::string &pinPackage,
                                        time_t ali,
                                        tABC_Error *pError);
/**
 * Create a git repository on the server, suitable for holding a wallet.
 */
Status
LoginServerWalletCreate(const Login &login, const std::string &syncKey);

/**
 * Lock the server wallet repository, so it is not automatically deleted.
 */
Status
LoginServerWalletActivate(const Login &login, const std::string &syncKey);

tABC_CC ABC_LoginServerOtpEnable(const Login &login, const std::string &otpToken, const long timeout, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpDisable(const Login &login, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpStatus(const Login &login, bool &on, long &timeout, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpReset(const Lobby &lobby, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpPending(std::list<DataChunk> users, std::list<bool> &isPending, tABC_Error *pError);
tABC_CC ABC_LoginServerOtpResetCancelPending(const Login &login, tABC_Error *pError);

tABC_CC ABC_LoginServerUploadLogs(const Account *account, tABC_Error *pError);

} // namespace abcd

#endif
