/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * PIN-based re-login logic.
 */

#ifndef ABCD_LOGIN_LOGIN_PIN_HPP
#define ABCD_LOGIN_LOGIN_PIN_HPP

#include "../../src/ABC.h"
#include <time.h>
#include <memory>

namespace abcd {

class Login;
class Lobby;

tABC_CC ABC_LoginPinExists(const char *szUserName,
                           bool *pbExists,
                           tABC_Error *pError);

tABC_CC ABC_LoginPinDelete(const char *szUserName,
                           tABC_Error *pError);

tABC_CC ABC_LoginPin(std::shared_ptr<Login> &result,
                     std::shared_ptr<Lobby> lobby,
                     const char *szPin,
                     tABC_Error *pError);

tABC_CC ABC_LoginPinSetup(Login &login,
                          const char *szPin,
                          time_t expires,
                          tABC_Error *pError);

} // namespace abcd

#endif
