/**
 * @file
 * AirBitz URL functions.
 *
 * This file contains all of the functions associated with sending and receiving
 * data to and from servers.
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

#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <pthread.h>
#include "ABC.h"
#include "ABC_FileIO.h"
#include "ABC_Util.h"
#include "ABC_Debug.h"
#include "ABC_URL.h"
#include "ABC_ServerDefs.h"

static char *gszCaCertPath = NULL;
static bool gbInitialized = false;
static pthread_mutex_t  gMutex; // to block multiple threads from accessing curl at the same time

static size_t ABC_URLCurlWriteData(void *pBuffer, size_t memberSize, size_t numMembers, void *pUserData);

/**
 * Initialize the URL system
 */
tABC_CC ABC_URLInitialize(const char *szCaCertPath, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "ABC_URL has already been initalized");

    // create a mutex to block multiple threads from accessing curl at the same time
    pthread_mutexattr_t mutexAttrib;
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_init(&mutexAttrib), ABC_CC_MutexError, "ABC_URL could not create mutex attribute");
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_settype(&mutexAttrib, PTHREAD_MUTEX_RECURSIVE), ABC_CC_MutexError, "ABC_URL could not set mutex attributes");
    ABC_CHECK_ASSERT(0 == pthread_mutex_init(&gMutex, &mutexAttrib), ABC_CC_MutexError, "ABC_URL could not create mutex");
    pthread_mutexattr_destroy(&mutexAttrib);

    // initialize curl
    CURLcode curlCode;
    if ((curlCode = curl_global_init(CURL_GLOBAL_ALL)) != 0)
    {
        ABC_DebugLog("Curl init failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl init failed");
    }
    if (szCaCertPath)
    {
        ABC_STRDUP(gszCaCertPath, szCaCertPath);
    }

    gbInitialized = true;

exit:

    return cc;
}

/**
 * Shut down the URL system
 */
void ABC_URLTerminate()
{
    if (gbInitialized == true)
    {
        // cleanup curl
        curl_global_cleanup();

        pthread_mutex_destroy(&gMutex);

        ABC_FREE_STR(gszCaCertPath);

        gbInitialized = false;
    }
}

/**
 * Makes a URL request.
 * @param szURL         The request URL.
 * @param pData         The location to store the results. The caller is responsible for free'ing this.
 */
tABC_CC ABC_URLRequest(const char *szURL, tABC_U08Buf *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf Data = ABC_BUF_NULL;
    CURL *pCurlHandle = NULL;

    ABC_CHECK_RET(ABC_URLMutexLock(pError));
    ABC_CHECK_NULL(szURL);
    ABC_CHECK_NULL(pData);

    // start with no data
    ABC_BUF_CLEAR(*pData);

    CURLcode curlCode = 0;

    ABC_CHECK_RET(ABC_URLCurlHandleInit(&pCurlHandle, pError))
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_CAINFO, gszCaCertPath)) == 0,
        ABC_CC_Error, "Curl failed to set ca-certificates.crt");

    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, szURL)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }


    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEFUNCTION, ABC_URLCurlWriteData)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEDATA, &Data)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    if ((curlCode = curl_easy_perform(pCurlHandle)) != 0)
    {
        ABC_DebugLog("Curl easy perform failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy perform failed");
    }

    // store the data in the user's buffer
    ABC_BUF_SET(*pData, Data);
    ABC_BUF_CLEAR(Data);

exit:
    ABC_BUF_FREE(Data);
    curl_easy_cleanup(pCurlHandle);

    ABC_URLMutexUnlock(NULL);
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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf Data;

    ABC_CHECK_RET(ABC_URLMutexLock(pError));
    ABC_CHECK_NULL(szURL);
    ABC_CHECK_NULL(pszResults);

    // make the request
    ABC_CHECK_RET(ABC_URLRequest(szURL, &Data, pError));

    // add the null
    ABC_BUF_APPEND_PTR(Data, "", 1);

    // assign the results
    *pszResults = (char *)ABC_BUF_PTR(Data);
    ABC_BUF_CLEAR(Data);

exit:
    ABC_BUF_FREE(Data);

    ABC_URLMutexUnlock(NULL);
    return cc;
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
tABC_CC ABC_URLPost(const char *szURL, const char *szPostData, tABC_U08Buf *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf Data = ABC_BUF_NULL;
    CURL *pCurlHandle = NULL;
    struct curl_slist *slist = NULL;

    ABC_CHECK_RET(ABC_URLMutexLock(pError));
    ABC_CHECK_NULL(szURL);
    ABC_CHECK_NULL(szPostData);
    ABC_CHECK_NULL(pData);

    // start with no data
    ABC_BUF_CLEAR(*pData);

    CURLcode curlCode = 0;
    ABC_CHECK_RET(ABC_URLCurlHandleInit(&pCurlHandle, pError))

    // Set the ca certificate
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_CAINFO, gszCaCertPath)) == 0,
        ABC_CC_Error, "Curl failed to set ca-certificates.crt");

    // set the URL
    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, szURL)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    // set the callback function for data that comes back
    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEFUNCTION, ABC_URLCurlWriteData)) != 0)
    {
        ABC_DebugLog("Curl easy setopt failed: %d\n", curlCode);
        ABC_RET_ERROR(ABC_CC_URLError, "Curl easy setopt failed");
    }

    // set the user data pointer that will be in the callback function
    if ((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEDATA, &Data)) != 0)
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

    // store the data in the user's buffer
    ABC_BUF_SET(*pData, Data);
    ABC_BUF_CLEAR(Data);

exit:
    ABC_BUF_FREE(Data);
    curl_easy_cleanup(pCurlHandle);
    curl_slist_free_all(slist);

    ABC_URLMutexUnlock(NULL);
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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf Data;

    ABC_CHECK_RET(ABC_URLMutexLock(pError));
    ABC_CHECK_NULL(szURL);
    ABC_CHECK_NULL(szPostData);
    ABC_CHECK_NULL(pszResults);

    // make the request
    ABC_CHECK_RET(ABC_URLPost(szURL, szPostData, &Data, pError));

    // add the null
    ABC_BUF_APPEND_PTR(Data, "", 1);

    // assign the results
    *pszResults = (char *)ABC_BUF_PTR(Data);
    ABC_BUF_CLEAR(Data);

exit:
    ABC_BUF_FREE(Data);

    ABC_URLMutexUnlock(NULL);
    return cc;
}

/**
 * Makes a URL post request and returns results in a string.
 * @param szURL         The request URL.
 * @param szPostData    The data to be posted in the request
 * @param pszResults    The location to store the allocated string with results.
 *                      The caller is responsible for free'ing this.
 */
tABC_CC ABC_URLCheckResults(const char *szResults, json_t **ppJSON_Result, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t error;
    json_t *pJSON_Root = NULL;
    json_t *pJSON_Value = NULL;

    pJSON_Root = json_loads(szResults, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing server JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // get the status code
    pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_STATUS_CODE_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_number(pJSON_Value)), ABC_CC_JSONError, "Error parsing server JSON status code");
    int statusCode = (int) json_integer_value(pJSON_Value);

    // if there was a failure
    if (ABC_Server_Code_Success != statusCode)
    {
        if (ABC_Server_Code_AccountExists == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_AccountAlreadyExists, "Account already exists on server");
        }
		else if (ABC_Server_Code_NoAccount == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_AccountDoesNotExist, "Account does not exist on server");
        }
        else if (ABC_Server_Code_InvalidPassword == statusCode)
        {
            ABC_RET_ERROR(ABC_CC_BadPassword, "Invalid password on server");
        }
        else
        {
            // get the message
            pJSON_Value = json_object_get(pJSON_Root, ABC_SERVER_JSON_MESSAGE_FIELD);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_DebugLog("Server message: %s", json_string_value(pJSON_Value));
            ABC_RET_ERROR(ABC_CC_ServerError, json_string_value(pJSON_Value));
        }
    }
	if (ppJSON_Result)
	{
		*ppJSON_Result = pJSON_Root;
		pJSON_Root = NULL;
	}
exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    return cc;
}

tABC_CC ABC_URLCurlHandleInit(CURL **ppCurlHandle, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    CURLcode curlCode;
    CURL *pCurlHandle = NULL;

    pCurlHandle = curl_easy_init();
    if (gszCaCertPath)
    {
        ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_CAINFO, gszCaCertPath)) == 0,
            ABC_CC_Error, "Curl failed to set ca-certificates.crt");
    }
    *ppCurlHandle = pCurlHandle;
exit:
    return cc;
}


/**
 * This is the function that gets called by CURL when it has data to be saved from a request.
 * @param pBuffer Pointer to incoming data
 * @param memberSize Size of the members
 * @param numMembers Number of members in the buffer
 * @param pUserData  User data specified initial calls
 */
static
size_t ABC_URLCurlWriteData(void *pBuffer, size_t memberSize, size_t numMembers, void *pUserData)
{
    tABC_U08Buf *pCurlBuffer = (tABC_U08Buf *)pUserData;
    unsigned int dataAvailLength = (unsigned int) numMembers * (unsigned int) memberSize;
    size_t amountWritten = 0;

    if (pCurlBuffer)
    {
        // if we don't have any buffer allocated yet
        if (ABC_BUF_PTR(*pCurlBuffer) == NULL)
        {
            ABC_BUF_DUP_PTR(*pCurlBuffer, pBuffer, dataAvailLength);
        }
        else
        {
            ABC_BUF_APPEND_PTR(*pCurlBuffer, pBuffer, dataAvailLength);
        }
        amountWritten = dataAvailLength;
    }

    return amountWritten;
}

/**
 * Locks the global mutex
 *
 */
tABC_CC ABC_URLMutexLock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_URL has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_lock(&gMutex), ABC_CC_MutexError, "ABC_URL error locking mutex");

exit:

    return cc;
}

/**
 * Unlocks the global mutex
 *
 */
tABC_CC ABC_URLMutexUnlock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_URL has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_unlock(&gMutex), ABC_CC_MutexError, "ABC_URL error unlocking mutex");

exit:

    return cc;
}
