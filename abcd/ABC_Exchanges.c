/**
 * @file
 * AirBitz Exchange functions.
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

#include "ABC_Exchanges.h"
#include "ABC_Login.h"
#include "ABC_Account.h"
#include "util/ABC_FileIO.h"
#include "util/ABC_URL.h"
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <pthread.h>

#define EXCHANGE_RATE_DIRECTORY "Exchanges"

#define BITSTAMP_RATE_URL "https://www.bitstamp.net/api/ticker/"
#define COINBASE_RATE_URL "https://coinbase.com/api/v1/currencies/exchange_rates"

typedef struct sABC_ExchangeCacheEntry
{
    int currencyNum;
    time_t last_updated;
    double exchange_rate;
} tABC_ExchangeCacheEntry;

static unsigned int gExchangesCacheCount = 0;
static tABC_ExchangeCacheEntry **gaExchangeCacheArray = NULL;

static bool gbInitialized = false;
static pthread_mutex_t gMutex;

static tABC_CC ABC_ExchangeGetRate(tABC_ExchangeInfo *pInfo, double *szRate, tABC_Error *pError);
static tABC_CC ABC_ExchangeNeedsUpdate(tABC_ExchangeInfo *pInfo, bool *bUpdateRequired, double *szRate, tABC_Error *pError);
static tABC_CC ABC_ExchangeBitStampRate(tABC_ExchangeInfo *pInfo, tABC_Error *pError);
static tABC_CC ABC_ExchangeCoinBaseRates(tABC_ExchangeInfo *pInfo, tABC_Error *pError);
static tABC_CC ABC_ExchangeExtractAndSave(json_t *pJSON_Root, const char *szField, int currencyNum, tABC_Error *pError);
static tABC_CC ABC_ExchangeGet(const char *szUrl, tABC_U08Buf *pData, tABC_Error *pError);
static tABC_CC ABC_ExchangeGetString(const char *szURL, char **pszResults, tABC_Error *pError);
static size_t  ABC_ExchangeCurlWriteData(void *pBuffer, size_t memberSize, size_t numMembers, void *pUserData);
static tABC_CC ABC_ExchangeGetFilename(char **pszFilename, int currencyNum, tABC_Error *pError);
static tABC_CC ABC_ExchangeExtractSource(tABC_ExchangeInfo *pInfo, char **szSource, tABC_Error *pError);
static tABC_CC ABC_ExchangeMutexLock(tABC_Error *pError);
static tABC_CC ABC_ExchangeMutexUnlock(tABC_Error *pError);

static tABC_CC ABC_ExchangeGetFromCache(int currencyNum, tABC_ExchangeCacheEntry **ppData, tABC_Error *pError);
static tABC_CC ABC_ExchangeClearCache(tABC_Error *pError);
static tABC_CC ABC_ExchangeAddToCache(tABC_ExchangeCacheEntry *pData, tABC_Error *pError);
static tABC_CC ABC_ExchangeAllocCacheEntry(tABC_ExchangeCacheEntry **ppCache, int currencyNum, time_t last_updated, double exchange_rate, tABC_Error *pError);
static void ABC_ExchangeFreeCacheEntry(tABC_ExchangeCacheEntry *pCache);


/**
 * Initialize the AirBitz Exchanges.
 *
 * @param pData                         Pointer to data to be returned back in callback
 * @param pError                        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ExchangeInitialize(tABC_Error                  *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "ABC_Exchanges has already been initalized");

    pthread_mutexattr_t mutexAttrib;
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_init(&mutexAttrib), ABC_CC_MutexError, "ABC_Exchanges could not create mutex attribute");
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_settype(&mutexAttrib, PTHREAD_MUTEX_RECURSIVE), ABC_CC_MutexError, "ABC_Exchanges could not set mutex attributes");
    ABC_CHECK_ASSERT(0 == pthread_mutex_init(&gMutex, &mutexAttrib), ABC_CC_MutexError, "ABC_Exchanges could not create mutex");
    pthread_mutexattr_destroy(&mutexAttrib);

    gbInitialized = true;
exit:
    return cc;
}

/**
 * Fetches the current rate and requests a new value if the current value is old.
 */
tABC_CC ABC_ExchangeCurrentRate(const char *szUserName, const char *szPassword,
                                int currencyNum, double *pRate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_ExchangeInfo *pInfo = NULL;
    tABC_ExchangeCacheEntry *pCache = NULL;

    ABC_CHECK_RET(ABC_ExchangeGetFromCache(currencyNum, &pCache, pError));
    if (pCache)
    {
        *pRate = pCache->exchange_rate;
    }
    else
    {
        ABC_CHECK_RET(ABC_ExchangeAlloc(szUserName, szPassword, currencyNum, NULL, NULL, &pInfo, pError));
        ABC_CHECK_RET(ABC_ExchangeGetRate(pInfo, pRate, pError));
        ABC_ExchangeFreeInfo(pInfo);
    }
exit:
    return cc;
}

void ABC_ExchangeTerminate()
{
    if (gbInitialized == true)
    {
        ABC_ExchangeClearCache(NULL);

        pthread_mutex_destroy(&gMutex);

        gbInitialized = false;
    }
}

tABC_CC ABC_ExchangeUpdate(tABC_ExchangeInfo *pInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szSource = NULL;
    double rate;
    bool bUpdateRequired = true;

    ABC_CHECK_RET(ABC_ExchangeNeedsUpdate(pInfo, &bUpdateRequired, &rate, pError));
    if (bUpdateRequired)
    {
        ABC_CHECK_RET(ABC_ExchangeExtractSource(pInfo, &szSource, pError));
        if (szSource)
        {
            if (strcmp(ABC_BITSTAMP, szSource) == 0)
            {
                ABC_CHECK_RET(ABC_ExchangeBitStampRate(pInfo, pError));
            }
            else if (strcmp(ABC_COINBASE, szSource) == 0)
            {
                ABC_CHECK_RET(ABC_ExchangeCoinBaseRates(pInfo, pError));
            }
        }
    }
exit:
    return cc;
}

void *ABC_ExchangeUpdateThreaded(void *pData)
{
    tABC_ExchangeInfo *pInfo = (tABC_ExchangeInfo *) pData;
    if (pInfo)
    {
        tABC_RequestResults results;
        memset(&results, 0, sizeof(tABC_RequestResults));

        results.requestType = ABC_RequestType_SendBitcoin;

        results.bSuccess = false;

        // send the transaction
        tABC_CC CC = ABC_ExchangeUpdate(pInfo, &(results.errorInfo));
        results.errorInfo.code = CC;

        // we are done so load up the info and ship it back to the caller via the callback
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);

        // it is our responsibility to free the info struct
        ABC_ExchangeFreeInfo(pInfo);
    }
    return NULL;
}


static
tABC_CC ABC_ExchangeGetRate(tABC_ExchangeInfo *pInfo, double *pRate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    bool bUpdateRequired = true; // Ignored

    ABC_CHECK_RET(ABC_ExchangeNeedsUpdate(pInfo, &bUpdateRequired, pRate, pError));
exit:
    return cc;
}

static
tABC_CC ABC_ExchangeNeedsUpdate(tABC_ExchangeInfo *pInfo, bool *bUpdateRequired, double *pRate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;
    char *szRate = NULL;
    tABC_ExchangeCacheEntry *pCache = NULL;
    time_t timeNow = time(NULL);
    bool bExists = false;

    ABC_CHECK_RET(ABC_ExchangeMutexLock(pError));
    ABC_CHECK_RET(ABC_ExchangeGetFromCache(pInfo->currencyNum, &pCache, pError));
    if (pCache)
    {
        if ((timeNow - pCache->last_updated) < ABC_EXCHANGE_RATE_REFRESH_INTERVAL_SECONDS)
        {
            *bUpdateRequired = false;
        }
        *pRate = pCache->exchange_rate;
    }
    else
    {
        ABC_CHECK_RET(ABC_ExchangeGetFilename(&szFilename, pInfo->currencyNum, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
        if (true == bExists)
        {
            ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, &szRate, pError));
            // Set the exchange rate
            *pRate = strtod(szRate, NULL);
            // get the time the file was last changed
            time_t timeFileMod;
            ABC_CHECK_RET(ABC_FileIOFileModTime(szFilename, &timeFileMod, pError));

            // if it isn't too old then don't update
            if ((timeNow - timeFileMod) < ABC_EXCHANGE_RATE_REFRESH_INTERVAL_SECONDS)
            {
                *bUpdateRequired = false;
            }
        }
        else
        {
            *bUpdateRequired = true;
            *pRate = 0.0;
        }
        ABC_CHECK_RET(ABC_ExchangeAllocCacheEntry(&pCache, pInfo->currencyNum,
                                                  timeNow, *pRate, pError));
        if (ABC_CC_WalletAlreadyExists == ABC_ExchangeAddToCache(pCache, pError))
        {
            ABC_ExchangeFreeCacheEntry(pCache);
        }
    }
exit:
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szRate);
    ABC_CHECK_RET(ABC_ExchangeMutexUnlock(NULL));
    return cc;
}

static
tABC_CC ABC_ExchangeBitStampRate(tABC_ExchangeInfo *pInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t error;
    json_t *pJSON_Root = NULL;
    char *szResponse = NULL;

    // Fetch exchanges from bitstamp
    ABC_CHECK_RET(ABC_ExchangeGetString(BITSTAMP_RATE_URL, &szResponse, pError));

    // Parse the json
    pJSON_Root = json_loads(szResponse, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // USD
    ABC_ExchangeExtractAndSave(pJSON_Root, "last", CURRENCY_NUM_USD, pError);
exit:
    ABC_FREE_STR(szResponse);
    if (pJSON_Root) json_decref(pJSON_Root);
    return cc;
}

static
tABC_CC ABC_ExchangeCoinBaseRates(tABC_ExchangeInfo *pInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t error;
    json_t *pJSON_Root = NULL;
    char *szResponse = NULL;

    // Fetch exchanges from coinbase
    ABC_CHECK_RET(ABC_ExchangeGetString(COINBASE_RATE_URL, &szResponse, pError));

    // Parse the json
    pJSON_Root = json_loads(szResponse, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // USD
    ABC_ExchangeExtractAndSave(pJSON_Root, "btc_to_usd", CURRENCY_NUM_USD, pError);
    // CAD
    ABC_ExchangeExtractAndSave(pJSON_Root, "btc_to_cad", CURRENCY_NUM_CAD, pError);
    // EUR
    ABC_ExchangeExtractAndSave(pJSON_Root, "btc_to_eur", CURRENCY_NUM_EUR, pError);
    // CUP
    ABC_ExchangeExtractAndSave(pJSON_Root, "btc_to_cup", CURRENCY_NUM_CUP, pError);
    // GBP
    ABC_ExchangeExtractAndSave(pJSON_Root, "btc_to_gbp", CURRENCY_NUM_GBP, pError);
    // MXN
    ABC_ExchangeExtractAndSave(pJSON_Root, "btc_to_mxn", CURRENCY_NUM_MXN, pError);
    // CNY
    ABC_ExchangeExtractAndSave(pJSON_Root, "btc_to_cny", CURRENCY_NUM_CNY, pError);
exit:
    ABC_FREE_STR(szResponse);
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

static
tABC_CC ABC_ExchangeExtractAndSave(json_t *pJSON_Root, const char *szField,
                                   int currencyNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;
    char *szValue = NULL;
    json_t *jsonVal = NULL;
    tABC_ExchangeCacheEntry *pCache = NULL;
    time_t timeNow = time(NULL);

    ABC_CHECK_RET(ABC_ExchangeMutexLock(pError));
    // Extract value from json
    jsonVal = json_object_get(pJSON_Root, szField);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON");
    ABC_STRDUP(szValue, json_string_value(jsonVal));

    ABC_DebugLog("Exchange Response: %s = %s\n", szField, szValue);
    // Right changes to disk
    ABC_CHECK_RET(ABC_ExchangeGetFilename(&szFilename, currencyNum, pError));
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szValue, pError));

    // Update the cache
    ABC_CHECK_RET(ABC_ExchangeAllocCacheEntry(&pCache, currencyNum, timeNow, strtod(szValue, NULL), pError));
    if (ABC_CC_WalletAlreadyExists == ABC_ExchangeAddToCache(pCache, pError))
    {
        ABC_ExchangeFreeCacheEntry(pCache);
    }
exit:
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szValue);

    ABC_CHECK_RET(ABC_ExchangeMutexUnlock(NULL));
    return cc;
}

static
tABC_CC ABC_ExchangeGet(const char *szUrl, tABC_U08Buf *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    CURL *pCurlHandle = NULL;
    CURLcode curlCode;
    long resCode;

    ABC_CHECK_RET(ABC_URLMutexLock(pError));

    ABC_CHECK_RET(ABC_URLCurlHandleInit(&pCurlHandle, pError))
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_SSL_VERIFYPEER, 1L)) == 0,
        ABC_CC_Error, "Unable to verify servers cert");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, szUrl)) == 0,
        ABC_CC_Error, "Curl failed to set URL\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEDATA, pData)) == 0,
        ABC_CC_Error, "Curl failed to set data\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEFUNCTION, ABC_ExchangeCurlWriteData)) == 0,
        ABC_CC_Error, "Curl failed to set callback\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_perform(pCurlHandle)) == 0,
        ABC_CC_Error, "Curl failed to perform\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_getinfo(pCurlHandle, CURLINFO_RESPONSE_CODE, &resCode)) == 0,
        ABC_CC_Error, "Curl failed to retrieve response info\n");
    ABC_CHECK_ASSERT(resCode == 200, ABC_CC_Error, "Response code should be 200");
exit:

    if (pCurlHandle != NULL)
        curl_easy_cleanup(pCurlHandle);

    ABC_CHECK_RET(ABC_URLMutexUnlock(pError));
    return cc;
}

static
tABC_CC ABC_ExchangeGetString(const char *szURL, char **pszResults, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf Data = ABC_BUF_NULL;

    ABC_CHECK_RET(ABC_URLMutexLock(pError));
    ABC_CHECK_NULL(szURL);
    ABC_CHECK_NULL(pszResults);

    // make the request
    ABC_CHECK_RET(ABC_ExchangeGet(szURL, &Data, pError));

    // add the null
    ABC_BUF_APPEND_PTR(Data, "", 1);

    // assign the results
    *pszResults = (char *)ABC_BUF_PTR(Data);
    ABC_BUF_CLEAR(Data);

exit:
    ABC_BUF_FREE(Data);

    ABC_CHECK_RET(ABC_URLMutexUnlock(pError));
    return cc;
}

static
size_t ABC_ExchangeCurlWriteData(void *pBuffer, size_t memberSize, size_t numMembers, void *pUserData)
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

static
tABC_CC ABC_ExchangeGetFilename(char **pszFilename, int currencyNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    char *szRoot     = NULL;
    char *szRateRoot = NULL;
    char *szFilename = NULL;
    bool bExists     = false;

    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;

    ABC_CHECK_RET(ABC_FileIOGetRootDir(&szRoot, pError));
    ABC_ALLOC(szRateRoot, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szRateRoot, "%s/%s", szRoot, EXCHANGE_RATE_DIRECTORY);
    ABC_CHECK_RET(ABC_FileIOFileExists(szRateRoot, &bExists, pError));
    if (true != bExists)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szRateRoot, pError));
    }

    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%d.txt", szRateRoot, currencyNum);
    *pszFilename = szFilename;
exit:
    ABC_FREE_STR(szRoot);
    ABC_FREE_STR(szRateRoot);
    return cc;
}

static
tABC_CC ABC_ExchangeExtractSource(tABC_ExchangeInfo *pInfo,
                                  char **szSource, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_SyncKeys *pKeys = NULL;
    tABC_AccountSettings *pAccountSettings = NULL;

    *szSource = NULL;
    if (pInfo->szUserName && pInfo->szPassword)
    {
        ABC_CHECK_RET(ABC_LoginGetSyncKeys(pInfo->szUserName, pInfo->szPassword, &pKeys, pError));
        ABC_AccountSettingsLoad(pKeys,
                                &pAccountSettings,
                                pError);
    }
    if (pAccountSettings)
    {
        tABC_ExchangeRateSources *pSources = &(pAccountSettings->exchangeRateSources);
        if (pSources->numSources > 0)
        {
            for (int i = 0; i < pSources->numSources; i++)
            {
                if (pSources->aSources[i]->currencyNum == pInfo->currencyNum)
                {
                    ABC_STRDUP(*szSource, pSources->aSources[i]->szSource);
                    break;
                }
            }
        }
    }
    if (!(*szSource))
    {
        // If the settings are not populated, defaults
        switch (pInfo->currencyNum)
        {
            case CURRENCY_NUM_USD:
                ABC_STRDUP(*szSource, ABC_BITSTAMP);
                break;
            case CURRENCY_NUM_CAD:
            case CURRENCY_NUM_CUP:
            case CURRENCY_NUM_CNY:
            case CURRENCY_NUM_EUR:
            case CURRENCY_NUM_GBP:
            case CURRENCY_NUM_MXN:
                ABC_STRDUP(*szSource, ABC_COINBASE);
                break;
            default:
                ABC_STRDUP(*szSource, ABC_BITSTAMP);
                break;
        }
    }
exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);
    ABC_FreeAccountSettings(pAccountSettings);

    return cc;
}

static
tABC_CC ABC_ExchangeMutexLock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_Exchanges has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_lock(&gMutex), ABC_CC_MutexError, "ABC_Exchanges error locking mutex");

exit:

    return cc;
}

static
tABC_CC ABC_ExchangeMutexUnlock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_Exchanges has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_unlock(&gMutex), ABC_CC_MutexError, "ABC_Exchanges error unlocking mutex");

exit:

    return cc;
}

/**
 * Clears all the data from the cache
 */
static
tABC_CC ABC_ExchangeClearCache(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_ExchangeMutexLock(pError));

    if ((gExchangesCacheCount > 0) && (NULL != gaExchangeCacheArray))
    {
        for (int i = 0; i < gExchangesCacheCount; i++)
        {
            tABC_ExchangeCacheEntry *pData = gaExchangeCacheArray[i];
            ABC_FREE(pData);
        }

        ABC_FREE(gaExchangeCacheArray);
        gExchangesCacheCount = 0;
    }

exit:

    ABC_ExchangeMutexUnlock(NULL);
    return cc;
}

static
tABC_CC ABC_ExchangeGetFromCache(int currencyNum, tABC_ExchangeCacheEntry **ppData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    // assume we didn't find it
    *ppData = NULL;

    if ((gExchangesCacheCount > 0) && (NULL != gaExchangeCacheArray))
    {
        for (int i = 0; i < gExchangesCacheCount; i++)
        {
            tABC_ExchangeCacheEntry *pData = gaExchangeCacheArray[i];
            if (currencyNum == pData->currencyNum)
            {
                // found it
                *ppData = pData;
                break;
            }
        }
    }
    return cc;
}

static
tABC_CC ABC_ExchangeAddToCache(tABC_ExchangeCacheEntry *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_ExchangeMutexLock(pError));
    ABC_CHECK_NULL(pData);

    // see if it exists first
    tABC_ExchangeCacheEntry *pExistingExchangeData = NULL;
    ABC_CHECK_RET(ABC_ExchangeGetFromCache(pData->currencyNum, &pExistingExchangeData, pError));

    // if it doesn't currently exist in the array
    if (pExistingExchangeData == NULL)
    {
        // if we don't have an array yet
        if ((gExchangesCacheCount == 0) || (NULL == gaExchangeCacheArray))
        {
            // create a new one
            gExchangesCacheCount = 0;
            ABC_ALLOC(gaExchangeCacheArray, sizeof(tABC_ExchangeCacheEntry *));
        }
        else
        {
            // extend the current one
            gaExchangeCacheArray = realloc(gaExchangeCacheArray, sizeof(tABC_ExchangeCacheEntry *) * (gExchangesCacheCount + 1));

        }
        gaExchangeCacheArray[gExchangesCacheCount] = pData;
        gExchangesCacheCount++;
    }
    else
    {
        pExistingExchangeData->last_updated = pData->last_updated;
        pExistingExchangeData->exchange_rate = pData->exchange_rate;
        cc = ABC_CC_WalletAlreadyExists;
    }

exit:

    ABC_ExchangeMutexUnlock(NULL);
    return cc;
}

tABC_CC ABC_ExchangeAlloc(const char *szUserName, const char *szPassword,
                          int currencyNum,
                          tABC_Request_Callback fRequestCallback, void *pData,
                          tABC_ExchangeInfo **ppInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_ExchangeInfo *pInfo;

    ABC_ALLOC(pInfo, sizeof(tABC_ExchangeInfo));
    ABC_STRDUP(pInfo->szUserName, szUserName);
    ABC_STRDUP(pInfo->szPassword, szPassword);
    pInfo->fRequestCallback = fRequestCallback;
    pInfo->pData = pData;
    pInfo->currencyNum = currencyNum;

    *ppInfo = pInfo;
exit:
    return cc;
}

void ABC_ExchangeFreeInfo(tABC_ExchangeInfo *pInfo)
{
    if (pInfo)
    {
        ABC_FREE_STR(pInfo->szUserName);
        ABC_FREE_STR(pInfo->szPassword);
        ABC_CLEAR_FREE(pInfo, sizeof(tABC_ExchangeInfo));
    }
}

static
tABC_CC ABC_ExchangeAllocCacheEntry(tABC_ExchangeCacheEntry **ppCache,
                                    int currencyNum, time_t last_updated,
                                    double exchange_rate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_ExchangeCacheEntry *pCache;

    ABC_ALLOC(pCache, sizeof(tABC_ExchangeCacheEntry));
    pCache->currencyNum = currencyNum;
    pCache->last_updated = last_updated;
    pCache->exchange_rate = exchange_rate;

    *ppCache = pCache;
exit:
    return cc;
}

static
void ABC_ExchangeFreeCacheEntry(tABC_ExchangeCacheEntry *pCache)
{
    if (pCache)
    {
        ABC_CLEAR_FREE(pCache, sizeof(tABC_ExchangeCacheEntry));
    }
}
