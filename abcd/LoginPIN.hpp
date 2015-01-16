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

#include "../src/ABC.h"
#include "Login.hpp"
#include <time.h>

namespace abcd {

    tABC_CC ABC_LoginPinExists(const char *szUserName,
                               bool *pbExists,
                               tABC_Error *pError);

    tABC_CC ABC_LoginPinDelete(const char *szUserName,
                               tABC_Error *pError);

    tABC_CC ABC_LoginPin(tABC_Login **ppSelf,
                         const char *szUserName,
                         const char *szPIN,
                         tABC_Error *pError);

    tABC_CC ABC_LoginPinSetup(tABC_Login *pSelf,
                              const char *szPIN,
                              time_t expires,
                              tABC_Error *pError);

} // namespace abcd

#endif
