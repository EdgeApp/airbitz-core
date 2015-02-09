/*
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 */
/**
 * @file
 * Functions for dealing with the contents of the account sync directory.
 */

#ifndef ABC_TwoFactory_h
#define ABC_TwoFactory_h

#include "Login.hpp"
#include "../util/Sync.hpp"
#include "../../src/ABC.h"
#include <vector>

namespace abcd {

tABC_CC ABC_TwoFactorInitialize(tABC_Error *pError);
void ABC_TwoFactorTerminate();

tABC_CC ABC_TwoFactorEnable(tABC_Login *pSelf,
                            tABC_U08Buf L1,
                            tABC_U08Buf LP1,
                            const long timeout,
                            tABC_Error *pError);
tABC_CC ABC_TwoFactorDisable(tABC_Login *pSelf,
                             tABC_U08Buf L1,
                             tABC_U08Buf LP1,
                             tABC_Error *pError);

/**
 * Looks up the status of a users otp, and if it is 'on', also returns the
 * timeout.
 *
 * @param L1        L1
 * @param LP1       LP1
 * @param on        indicates if the user has Otp on
 * @param timeout   if Otp is on, includes the timeout in seconds
 */
tABC_CC ABC_LoginServerOtpStatus(tABC_U08Buf L1, tABC_U08Buf LP1,
    bool *on, long *timeout, tABC_Error *pError);

tABC_CC ABC_TwoFactorCacheSecret(tABC_Login *pSelf, tABC_Error *pError);

// Get/set token from client:
tABC_CC ABC_TwoFactorGetSecret(tABC_Login *pSelf,
                               char **pszSecret,
                               tABC_Error *pError);

tABC_CC ABC_TwoFactorGetQrCode(tABC_Login *pSelf, unsigned char **paData,
    unsigned int *pWidth, tABC_Error *pError);

tABC_CC ABC_TwoFactorSetSecret(tABC_Login *pSelf,
                               const char *szSecret,
                               tABC_Error *pError);

// Reset mechanism:
tABC_CC ABC_TwoFactorReset(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_Error *pError);

/**
 * Given a list of users, this function returns a list of bools indicating
 * whether there a pending reset request.
 *
 * @param users      vector of LocalLogins
 * @param isPending  matching  to LocalLogins
 */
tABC_CC ABC_TwoFactorPending(std::vector<tABC_U08Buf>& users,
    std::vector<bool>& isPending, tABC_Error *pError);

tABC_CC ABC_TwoFactorResetPending(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_Error *pError);
tABC_CC ABC_TwoFactorCancelPending(tABC_U08Buf L1, tABC_U08Buf LP1, tABC_Error *pError);

// Get TOTP for use with server requests
tABC_CC ABC_TwoFactorGetToken(char **pszToken, tABC_Error *pError);

} // namespace abcd

#endif
