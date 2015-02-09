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

tABC_CC ABC_LoginGetRQ(const char *szUserName,
                       char **pszRecoveryQuestions,
                       tABC_Error *pError);

tABC_CC ABC_LoginRecovery(tABC_Login **ppSelf,
                          const char *szUserName,
                          const char *szRecoveryAnswers,
                          tABC_Error *pError);

tABC_CC ABC_LoginRecoverySet(tABC_Login *pSelf,
                             const char *szRecoveryQuestions,
                             const char *szRecoveryAnswers,
                             tABC_Error *pError);

} // namespace abcd

#endif
