/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "ExchangeServers.hpp"
#include "Exchange.hpp"
#include "../util/URL.hpp"
#include <stdlib.h>
#include <curl/curl.h>

namespace abcd {

#define BITSTAMP_RATE_URL "https://www.bitstamp.net/api/ticker/"
#define COINBASE_RATE_URL "https://coinbase.com/api/v1/currencies/exchange_rates"
#define BNC_RATE_URL      "http://api.bravenewcoin.com/ticker/"

static
tABC_CC ABC_ExchangeCoinBaseMap(int currencyNum, std::string& field, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    switch (currencyNum)
    {
        case CURRENCY_NUM_USD:
            field = "btc_to_usd";
            break;
        case CURRENCY_NUM_CAD:
            field = "btc_to_cad";
            break;
        case CURRENCY_NUM_EUR:
            field = "btc_to_eur";
            break;
        case CURRENCY_NUM_CUP:
            field = "btc_to_cup";
            break;
        case CURRENCY_NUM_GBP:
            field = "btc_to_gbp";
            break;
        case CURRENCY_NUM_MXN:
            field = "btc_to_mxn";
            break;
        case CURRENCY_NUM_CNY:
            field = "btc_to_cny";
            break;
        case CURRENCY_NUM_AUD:
            field = "btc_to_aud";
            break;
        case CURRENCY_NUM_PHP:
            field = "btc_to_php";
            break;
        case CURRENCY_NUM_HKD:
            field = "btc_to_hkd";
            break;
        case CURRENCY_NUM_NZD:
            field = "btc_to_nzd";
            break;
        default:
            ABC_CHECK_ASSERT(false, ABC_CC_Error, "Unsupported currency");
    }
exit:
    return cc;
}

static
tABC_CC ABC_ExchangeBncMap(int currencyNum, std::string& url, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    url += BNC_RATE_URL;
    switch (currencyNum) {
        case CURRENCY_NUM_USD:
            url += "bnc_ticker_btc_usd.json";
            break;
        case CURRENCY_NUM_AUD:
            url += "bnc_ticker_btc_aud.json";
            break;
        case CURRENCY_NUM_CAD:
            url += "bnc_ticker_btc_cad.json";
            break;
        case CURRENCY_NUM_CNY:
            url += "bnc_ticker_btc_cny.json";
            break;
        case CURRENCY_NUM_HKD:
            url += "bnc_ticker_btc_hkd.json";
            break;
        case CURRENCY_NUM_MXN:
            url += "bnc_ticker_btc_mxn.json";
            break;
        case CURRENCY_NUM_NZD:
            url += "bnc_ticker_btc_nzd.json";
            break;
        case CURRENCY_NUM_GBP:
            url += "bnc_ticker_btc_gbp.json";
            break;
        case CURRENCY_NUM_EUR:
            url += "bnc_ticker_btc_eur.json";
            break;
        default:
            ABC_CHECK_ASSERT(false, ABC_CC_Error, "Unsupported currency");
    }
exit:
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
tABC_CC ABC_ExchangeGet(const char *szUrl, tABC_U08Buf *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCurlLock lock(gCurlMutex);
    CURL *pCurlHandle = NULL;
    CURLcode curlCode;
    long resCode;

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

    return cc;
}

static
tABC_CC ABC_ExchangeGetString(const char *szURL, char **pszResults, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCurlLock lock(gCurlMutex);

    AutoU08Buf Data;

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
    return cc;
}

static
tABC_CC ABC_ExchangeExtract(json_t *pJSON_Root, const char *szField,
                                   int currencyNum, double &rate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szValue = NULL;
    json_t *jsonVal = NULL;

    // Extract value from json
    jsonVal = json_object_get(pJSON_Root, szField);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON");
    ABC_STRDUP(szValue, json_string_value(jsonVal));

    ABC_DebugLog("Exchange Response: %s = %s\n", szField, szValue);
    rate = strtod(szValue, nullptr);

exit:
    ABC_FREE_STR(szValue);

    return cc;
}

tABC_CC ABC_ExchangeBitStampRate(int currencyNum, double &rate, tABC_Error *pError)
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
    ABC_ExchangeExtract(pJSON_Root, "last", CURRENCY_NUM_USD, rate, pError);
exit:
    ABC_FREE_STR(szResponse);
    if (pJSON_Root) json_decref(pJSON_Root);
    return cc;
}

tABC_CC ABC_ExchangeCoinBaseRates(int currencyNum, double &rate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t error;
    json_t *pJSON_Root = NULL;
    char *szResponse = NULL;
    std::string field;

    // Fetch exchanges from coinbase
    ABC_CHECK_RET(ABC_ExchangeGetString(COINBASE_RATE_URL, &szResponse, pError));

    // Parse the json
    pJSON_Root = json_loads(szResponse, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    ABC_CHECK_RET(ABC_ExchangeCoinBaseMap(currencyNum, field, pError));
    ABC_ExchangeExtract(pJSON_Root, field.c_str(), currencyNum, rate, pError);
exit:
    ABC_FREE_STR(szResponse);
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

tABC_CC ABC_ExchangeBncRates(int currencyNum, double &rate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t error;
    json_t *pJSON_Root = NULL;
    char *szResponse = NULL;

    std::string url;
    ABC_CHECK_RET(ABC_ExchangeBncMap(currencyNum, url, pError));
    ABC_CHECK_RET(ABC_ExchangeGetString(url.c_str(), &szResponse, pError));

    // Parse the json
    pJSON_Root = json_loads(szResponse, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    ABC_ExchangeExtract(pJSON_Root, "last_price", currencyNum, rate, pError);
exit:
    ABC_FREE_STR(szResponse);
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

} // namespace abcd
