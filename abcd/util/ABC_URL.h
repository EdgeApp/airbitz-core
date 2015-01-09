/**
 * @file
 * AirBitz URL function prototypes
 *
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
 *
 *  @author See AUTHORS
 *  @version 1.0
 */

#ifndef ABC_URL_h
#define ABC_URL_h

#include "ABC.h"
#include "ABC_Util.h"
#include <curl/curl.h>

namespace abcd {

#define ABC_URL_MAX_PATH_LENGTH 2048

    tABC_CC ABC_URLInitialize(const char *szCaCertPath, tABC_Error *pError);

    void ABC_URLTerminate();

    tABC_CC ABC_URLRequest(const char *szURL,
                           tABC_U08Buf *pData,
                           tABC_Error *pError);

    tABC_CC ABC_URLRequestString(const char *szURL,
                                 char **pszResults,
                                 tABC_Error *pError);

    tABC_CC ABC_URLPost(const char *szURL,
                        const char *szPostData,
                        tABC_U08Buf *pData,
                        tABC_Error *pError);

    tABC_CC ABC_URLPostString(const char *szURL,
                              const char *szPostData,
                              char **pszResults,
                              tABC_Error *pError);

    tABC_CC ABC_URLCheckResults(const char *szResults, json_t **ppJSON_Result, tABC_Error *pError);

    tABC_CC ABC_URLCurlHandleInit(CURL **ppCurlHandle, tABC_Error *pError);

    tABC_CC ABC_URLMutexLock(tABC_Error *pError);
    tABC_CC ABC_URLMutexUnlock(tABC_Error *pError);

} // namespace abcd

#endif
