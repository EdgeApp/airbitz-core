/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Recovery-question login logic.
 */

#ifndef ABCD_LOGIN_LOGIN_RECOVERY_HPP
#define ABCD_LOGIN_LOGIN_RECOVERY_HPP

#include "../../src/ABC.h"
#include <memory>

namespace abcd {

class Login;
class Lobby;

tABC_CC ABC_LoginGetRQ(Lobby &lobby,
                       char **pszRecoveryQuestions,
                       tABC_Error *pError);

tABC_CC ABC_LoginRecovery(std::shared_ptr<Login> &result,
                          std::shared_ptr<Lobby> lobby,
                          const char *szRecoveryAnswers,
                          tABC_Error *pError);

tABC_CC ABC_LoginRecoverySet(Login &login,
                             const char *szRecoveryQuestions,
                             const char *szRecoveryAnswers,
                             tABC_Error *pError);

} // namespace abcd

#endif
