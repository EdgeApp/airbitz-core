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

#ifndef ABC_LoginPassword_h
#define ABC_LoginPassword_h

#include "../src/ABC.h"
#include "Login.hpp"

namespace abcd {

tABC_CC ABC_LoginPassword(tABC_Login **ppSelf,
                          const char *szUserName,
                          const char *szPassword,
                          tABC_Error *pError);

tABC_CC ABC_LoginPasswordSet(tABC_Login *pSelf,
                             const char *szPassword,
                             tABC_Error *pError);

tABC_CC ABC_LoginPasswordOk(tABC_Login *pSelf,
                            const char *szPassword,
                            bool *pOk,
                            tABC_Error *pError);

} // namespace abcd

#endif
