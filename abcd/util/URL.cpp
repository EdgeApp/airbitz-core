/**
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

#include "URL.hpp"
#include "Debug.hpp"
#include "FileIO.hpp"
#include "Pin.hpp"
#include "Util.hpp"
#include "../config.h"
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <openssl/ssl.h>

namespace abcd {

#define URL_CONN_TIMEOUT 10

extern std::string gCertPath;

static CURLcode ABC_URLSSLCallback(CURL *curl, void *ssl_ctx, void *userptr);

/**
 * Curl data-storage callback.
 */
static size_t
curlWriteData(void *data, size_t memberSize, size_t numMembers, void *userData)
{
    auto size = numMembers * memberSize;

    auto string = static_cast<std::string *>(userData);
    string->append(static_cast<char *>(data), size);

    return size;
}

/**
 * Makes a URL request.
 * @param szURL         The request URL.
 * @param pData         The location to store the results. The caller is responsible for free'ing this.
 */
tABC_CC ABC_URLRequest(const char *szURL, std::string &reply, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    CURLcode curlCode = CURLE_OK;
    CURL *pCurlHandle = NULL;

    ABC_CHECK_NULL(szURL);

    // start with no data
    reply.clear();

    ABC_CHECK_RET(ABC_URLCurlHandleInit(&pCurlHandle, pError))

    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, szURL)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEFUNCTION, curlWriteData)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEDATA, &reply)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    if ((curlCode = curl_easy_perform(pCurlHandle)) != 0)
    {
        ABC_DebugLog("Curl easy perform failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy perform failed");
    }

exit:
    curl_easy_cleanup(pCurlHandle);

    return cc;
}

/**
 * Makes a URL request and returns results in a string.
 * @param szURL         The request URL.
 * @param pszResults    The location to store the allocated string with results.
 *                      The caller is responsible for free'ing this.
 */
tABC_CC ABC_URLRequestString(const char *szURL,
                             char **pszResults,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string reply;

    ABC_CHECK_NULL(szURL);
    ABC_CHECK_NULL(pszResults);

    // make the request
    ABC_CHECK_RET(ABC_URLRequest(szURL, reply, pError));

    // assign the results
    ABC_STRDUP(*pszResults, reply.c_str());

exit:
    return cc;
}

static
CURLcode ABC_URLSSLCallback(CURL *curl, void *ssl_ctx, void *userptr)
{
    SSL_CTX_set_verify((SSL_CTX *)ssl_ctx,
        SSL_VERIFY_PEER|SSL_VERIFY_CLIENT_ONCE, ABC_PinCertCallback);
    return CURLE_OK;
}

/**
 * Makes a URL post request.
 *
 * Note: that the content type on the post will be set to "Content-Type: application/json"
 *       and the api key is set to that specified in the server defs
 *
 * @param szURL         The request URL.
 * @param szPostData    The data to be posted in the request
 * @param pData         The location to store the results. The caller is responsible for free'ing this.
 */
tABC_CC ABC_URLPost(const char *szURL, const char *szPostData, std::string &reply, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    CURL *pCurlHandle = NULL;
    struct curl_slist *slist = NULL;
    CURLcode curlCode = CURLE_OK;

    ABC_CHECK_NULL(szURL);
    ABC_CHECK_NULL(szPostData);

    // start with no data
    reply.clear();

    ABC_CHECK_RET(ABC_URLCurlHandleInit(&pCurlHandle, pError))

    // Set the ca certificate
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_SSL_CTX_FUNCTION, ABC_URLSSLCallback)) == 0,
        ABC_CC_Error, "Curl failed to set ssl callback");

    // set the URL
    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, szURL)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    // set the callback function for data that comes back
    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEFUNCTION, curlWriteData)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    // set the user data pointer that will be in the callback function
    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEDATA, &reply)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    // set the post data
    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_POSTFIELDS, szPostData)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    // set the content type and the api key
    slist = curl_slist_append(slist, "Content-Type: application/json");
    slist = curl_slist_append(slist, API_KEY_HEADER);
    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_HTTPHEADER, slist)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    // execute the command
    if ((curlCode = curl_easy_perform(pCurlHandle)) != 0)
    {
        ABC_DebugLog("Curl easy perform failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy perform failed");
    }

exit:
    curl_easy_cleanup(pCurlHandle);
    curl_slist_free_all(slist);

    return cc;
}

/**
 * Makes a URL post request and returns results in a string.
 * @param szURL         The request URL.
 * @param szPostData    The data to be posted in the request
 * @param pszResults    The location to store the allocated string with results.
 *                      The caller is responsible for free'ing this.
 */
tABC_CC ABC_URLPostString(const char *szURL,
                          const char *szPostData,
                          char **pszResults,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string reply;

    ABC_CHECK_NULL(szURL);
    ABC_CHECK_NULL(szPostData);
    ABC_CHECK_NULL(pszResults);

    // make the request
    ABC_CHECK_RET(ABC_URLPost(szURL, szPostData, reply, pError));

    // assign the results
    ABC_STRDUP(*pszResults, reply.c_str());

exit:
    return cc;
}

tABC_CC ABC_URLCurlHandleInit(CURL **ppCurlHandle, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    CURLcode curlCode;
    CURL *pCurlHandle = NULL;

    pCurlHandle = curl_easy_init();
    if (!gCertPath.empty())
    {
        ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_CAINFO, gCertPath.c_str())) == 0,
            ABC_CC_Error, "Curl failed to set ca-certificates.crt");
    }
    curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_NOSIGNAL, 1);
    ABC_CHECK_ASSERT(curlCode == 0, ABC_CC_Error, "Unable to ignore signals");

    curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_CONNECTTIMEOUT, URL_CONN_TIMEOUT);
    ABC_CHECK_ASSERT(curlCode == 0, ABC_CC_Error, "Unable to set connection timeout");

    *ppCurlHandle = pCurlHandle;
exit:
    return cc;
}

} // namespace abcd
