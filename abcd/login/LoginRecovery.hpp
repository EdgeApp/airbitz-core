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

#ifndef ABC_LoginRecovery_h
#define ABC_LoginRecovery_h

#include "Login.hpp"
#include "../../src/ABC.h"

namespace abcd {

tABC_CC ABC_LoginGetRQ(Lobby &lobby,
                       char **pszRecoveryQuestions,
                       tABC_Error *pError);

tABC_CC ABC_LoginRecovery(Login *&result,
                          Lobby *lobby,
                          const char *szRecoveryAnswers,
                          tABC_Error *pError);

tABC_CC ABC_LoginRecoverySet(Login &login,
                             const char *szRecoveryQuestions,
                             const char *szRecoveryAnswers,
                             tABC_Error *pError);

} // namespace abcd

#endif
