/**
 * @file
 * AirBitz Exchange functions.
 *
 * @author Tim Horton
 * @version 1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <pthread.h>

#include "ABC_Exchanges.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Account.h"

#define BITSTAMP_RATE_URL "https://www.bitstamp.net/api/ticker/"
#define COINBASE_RATE_URL "https://coinbase.com/api/v1/currencies/exchange_rates"

static bool gbInitialized = false;
static pthread_mutex_t  gMutex;

static tABC_BitCoin_Event_Callback gfAsyncBitCoinEventCallback = NULL;
static void *pAsyncBitCoinCallerData = NULL;

static tABC_CC ABC_ExchangeGetRate(tABC_ExchangeInfo *pInfo, char **szRate, tABC_Error *pError);
static tABC_CC ABC_ExchangeNeedsUpdate(tABC_ExchangeInfo *pInfo, bool *bUpdateRequired, char **szRate, tABC_Error *pError);
static void    *ABC_ExchangeUpdateThreaded(void *pData);
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

/**
 * Initialize the AirBitz Exchanges.
 *
 * @param fAsyncBitCoinEventCallback    The function that should be called when there is an asynchronous
 *                                      BitCoin event
 * @param pData                         Pointer to data to be returned back in callback
 * @param pError                        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ExchangeInitialize(tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                               void                        *pData,
                               tABC_Error                  *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "ABC_Exchanges has already been initalized");

    pthread_mutexattr_t mutexAttrib;
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_init(&mutexAttrib), ABC_CC_MutexError, "ABC_Exchanges could not create mutex attribute");
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_settype(&mutexAttrib, PTHREAD_MUTEX_RECURSIVE), ABC_CC_MutexError, "ABC_Exchanges could not set mutex attributes");
    ABC_CHECK_ASSERT(0 == pthread_mutex_init(&gMutex, &mutexAttrib), ABC_CC_MutexError, "ABC_Exchanges could not create mutex");
    pthread_mutexattr_destroy(&mutexAttrib);

    gfAsyncBitCoinEventCallback = fAsyncBitCoinEventCallback;
    pAsyncBitCoinCallerData = pData;

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
    char *szRate = NULL;

    tABC_ExchangeInfo *info = malloc(sizeof(tABC_ExchangeInfo));
    info->exchange = ABC_BitStamp;
    info->currencyNum = currencyNum;
    ABC_STRDUP(info->szUserName, szUserName);
    ABC_STRDUP(info->szPassword, szPassword);
    ABC_CHECK_RET(ABC_ExchangeGetRate(info, &szRate, pError));

    *pRate = strtod(szRate, NULL);
exit:
    ABC_FREE_STR(szRate);
    return cc;
}

void ABC_ExchangeTerminate()
{
    if (gbInitialized == true)
    {
        pthread_mutex_destroy(&gMutex);

        gbInitialized = false;
    }
}

static
tABC_CC ABC_ExchangeGetRate(tABC_ExchangeInfo *pInfo, char **szRate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    bool bUpdateRequired = true;
    *szRate = NULL;

    ABC_CHECK_RET(ABC_ExchangeNeedsUpdate(pInfo, &bUpdateRequired, szRate, pError));
    if (bUpdateRequired)
    {
        pthread_t handle;
        if (!pthread_create(&handle, NULL, ABC_ExchangeUpdateThreaded, pInfo))
        {
            pthread_detach(handle);
        }
    }
    else
    {
        ABC_FREE_STR(pInfo->szUserName);
        ABC_FREE_STR(pInfo->szPassword);
        ABC_FREE(pInfo);
    }
exit:
    return cc;
}

static
tABC_CC ABC_ExchangeNeedsUpdate(tABC_ExchangeInfo *pInfo, bool *bUpdateRequired, char **szRate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;
    bool bExists = false;

    ABC_CHECK_RET(ABC_ExchangeMutexLock(pError));
    ABC_CHECK_RET(ABC_ExchangeGetFilename(&szFilename, pInfo->currencyNum, pError));
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    if (true == bExists)
    {
        ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, szRate, pError));
        // get the current time
        time_t timeNow = time(NULL);

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
        char *def = "0.0";
        ABC_STRDUP(*szRate, def);
    }
exit:
    ABC_FREE_STR(szFilename);
    ABC_CHECK_RET(ABC_ExchangeMutexUnlock(NULL));
    return cc;
}

static
void *ABC_ExchangeUpdateThreaded(void *pData)
{
    tABC_CC cc;
    tABC_Error error;
    tABC_Error *pError = &error;
    char *szSource = NULL, *szRate = NULL;
    tABC_ExchangeInfo *pInfo = (tABC_ExchangeInfo *) pData;
    bool bUpdateRequired = true;

    ABC_CHECK_RET(ABC_ExchangeMutexLock(pError));
    ABC_CHECK_RET(ABC_ExchangeNeedsUpdate(pInfo, &bUpdateRequired, &szRate, pError));
    if (bUpdateRequired)
    {
        ABC_CHECK_RET(ABC_ExchangeExtractSource(pInfo, &szSource, pError));
        if (szSource)
        {
            if (strcmp(ABC_BITSTAMP, szSource) == 0)
            {
                ABC_CHECK_RET(ABC_ExchangeBitStampRate(pInfo, &error));
            }
            else if (strcmp(ABC_COINBASE, szSource) == 0)
            {
                ABC_CHECK_RET(ABC_ExchangeCoinBaseRates(pInfo, &error));
            }

            if (gfAsyncBitCoinEventCallback)
            {
                tABC_AsyncBitCoinInfo resp;
                resp.eventType = ABC_AsyncEventType_ExchangeRateUpdate;
                ABC_STRDUP(resp.szDescription, "Exchange rate update");
                gfAsyncBitCoinEventCallback(&resp);
                ABC_FREE_STR(resp.szDescription);
            }
        }
    }
exit:
    ABC_FREE_STR(pInfo->szUserName);
    ABC_FREE_STR(pInfo->szPassword);
    ABC_FREE_STR(szSource);
    ABC_FREE_STR(szRate);
    ABC_FREE(pInfo);

    ABC_CHECK_RET(ABC_ExchangeMutexUnlock(NULL));
    return NULL;
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
    ABC_FREE_STR(szResponse);
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

    jsonVal = json_object_get(pJSON_Root, szField);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON");
    ABC_STRDUP(szValue, json_string_value(jsonVal));

    ABC_DebugLog("Exchange Response: %s = %s\n", szField, szValue); 

    ABC_CHECK_RET(ABC_ExchangeGetFilename(&szFilename, currencyNum, pError));
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szValue, pError));
exit:
    ABC_FREE_STR(szFilename);
    ABC_FREE_STR(szValue);

    return cc;
}

static
tABC_CC ABC_ExchangeGet(const char *szUrl, tABC_U08Buf *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    CURL *pCurlHandle = NULL;
    CURLcode curlCode;
    long resCode;

    pCurlHandle = curl_easy_init();
    // TODO: remove this!!!! and load the cacert for the platform you are on
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_SSL_VERIFYPEER, 0L)) == 0,
            ABC_CC_Error, "Failed to ignore peer verify");

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

    return cc;
}

static
tABC_CC ABC_ExchangeGetString(const char *szURL, char **pszResults, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf Data = ABC_BUF_NULL;

    ABC_CHECK_RET(ABC_ExchangeMutexLock(pError));
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
    ABC_ExchangeMutexUnlock(NULL);
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
    char *szRoot = NULL;

    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;

    ABC_CHECK_RET(ABC_FileIOGetRootDir(&szRoot, pError));
    ABC_ALLOC(*pszFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszFilename, "%s/%d.txt", szRoot, currencyNum);
exit:
    ABC_FREE_STR(szRoot);
    return cc;
}

static
tABC_CC ABC_ExchangeExtractSource(tABC_ExchangeInfo *pInfo,
                                  char **szSource, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_AccountSettings *pAccountSettings = NULL;

    *szSource = NULL;
    ABC_AccountLoadSettings(pInfo->szUserName,
                            pInfo->szPassword,
                            &pAccountSettings,
                            pError);
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
        // If the settings are populated, defaults
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

