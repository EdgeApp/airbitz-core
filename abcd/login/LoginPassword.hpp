/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Password-based login logic.
 */

#ifndef ABCD_LOGIN_LOGIN_PASSWORD_HPP
#define ABCD_LOGIN_LOGIN_PASSWORD_HPP

#include "../../src/ABC.h"
#include <memory>

namespace abcd {

class Login;
class Lobby;

tABC_CC ABC_LoginPassword(std::shared_ptr<Login> &result,
                          std::shared_ptr<Lobby> lobby,
                          const char *szPassword,
                          tABC_Error *pError);

tABC_CC ABC_LoginPasswordSet(Login &login,
                             const char *szPassword,
                             tABC_Error *pError);

tABC_CC ABC_LoginPasswordOk(Login &login,
                            const char *szPassword,
                            bool *pOk,
                            tABC_Error *pError);

} // namespace abcd

#endif
