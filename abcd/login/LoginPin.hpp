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

#ifndef ABC_LoginPin_h
#define ABC_LoginPin_h

#include "Login.hpp"
#include "../../src/ABC.h"
#include <time.h>

namespace abcd {

tABC_CC ABC_LoginPinExists(const char *szUserName,
                           bool *pbExists,
                           tABC_Error *pError);

tABC_CC ABC_LoginPinDelete(const char *szUserName,
                           tABC_Error *pError);

tABC_CC ABC_LoginPin(Login *&result,
                     Lobby *lobby,
                     const char *szPin,
                     tABC_Error *pError);

tABC_CC ABC_LoginPinSetup(Login &login,
                          const char *szPin,
                          time_t expires,
                          tABC_Error *pError);

} // namespace abcd

#endif
