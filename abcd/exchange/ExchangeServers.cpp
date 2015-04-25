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

#include <map>

namespace abcd {

#define BITSTAMP_RATE_URL "https://www.bitstamp.net/api/ticker/"
#define COINBASE_RATE_URL "https://coinbase.com/api/v1/currencies/exchange_rates"
#define BNC_GLOBAL_PRICE  "http://api.bravenewcoin.com/ticker/bnc_ticker_btc.json"
#define BNC_GLOBAL_RATE   "http://api.bravenewcoin.com/rates.json"

#define BNC_TIMEOUT 60

static time_t bncFetched = 0;
static std::map<std::string, std::string> bncRateCache;
static double bncGlobalPrice = 0.0;

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

static
tABC_CC ABC_ExchangeGet(const char *szUrl, std::string &reply, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCurlLock lock(gCurlMutex);
    CURL *pCurlHandle = NULL;
    CURLcode curlCode;
    long resCode;

    ABC_CHECK_NULL(szUrl);

    ABC_CHECK_RET(ABC_URLCurlHandleInit(&pCurlHandle, pError))
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_SSL_VERIFYPEER, 1L)) == 0,
        ABC_CC_Error, "Unable to verify servers cert");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, szUrl)) == 0,
        ABC_CC_Error, "Curl failed to set URL\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEDATA, &reply)) == 0,
        ABC_CC_Error, "Curl failed to set data\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEFUNCTION, curlWriteData)) == 0,
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

static
tABC_CC ABC_ExchangeBncCachePrices(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t error;
    json_t *pJSON_Root = NULL, *pJSON_Object, *pJSON_Row, *jsonVal;
    size_t rateCount = 0;
    std::string bncRateReply;
    std::string bncGlobalReply;

    // Fetch rates relative to USD
    ABC_CHECK_RET(ABC_ExchangeGet(BNC_GLOBAL_RATE, bncRateReply, pError));

    pJSON_Root = json_loads(bncRateReply.c_str(), 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    pJSON_Object = json_object_get(pJSON_Root, "rates");
    ABC_CHECK_ASSERT(pJSON_Object != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_array(pJSON_Object), ABC_CC_JSONError, "Error parsing JSON");

    rateCount = (size_t) json_array_size(pJSON_Object);
    for (unsigned i = 0; i < rateCount; i++)
    {
        pJSON_Row = json_array_get(pJSON_Object, i);
        ABC_CHECK_ASSERT((pJSON_Row && json_is_object(pJSON_Row)), ABC_CC_JSONError, "Error parsing JSON array element object");

        jsonVal = json_object_get(pJSON_Row, "id_currency");
        ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON");
        std::string symbol(json_string_value(jsonVal));

        jsonVal = json_object_get(pJSON_Row, "rate");
        ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON");
        bncRateCache[symbol] = std::string(json_string_value(jsonVal));
    }
    if (pJSON_Root) json_decref(pJSON_Root);

    // Fetch the global price index
    ABC_CHECK_RET(ABC_ExchangeGet(BNC_GLOBAL_PRICE, bncGlobalReply, pError));

    pJSON_Root = json_loads(bncGlobalReply.c_str(), 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    pJSON_Object = json_object_get(pJSON_Root, "ticker");
    ABC_CHECK_ASSERT(pJSON_Object != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Object), ABC_CC_JSONError, "Error parsing JSON");

    jsonVal = json_object_get(pJSON_Object, "bnc_price_index_usd");
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON");
    bncGlobalPrice = strtod(json_string_value(jsonVal), nullptr);

    // Update cache time
    bncFetched = time(NULL);

exit:
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

static
tABC_CC ABC_ExchangeBncGlobalPrice(const char *symbol, double &rate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    double rateValue = 0.0;

    if ((time(NULL) - bncFetched) > BNC_TIMEOUT)
        ABC_CHECK_RET(ABC_ExchangeBncCachePrices(pError));

    if (bncRateCache.find(symbol) != bncRateCache.end())
        rateValue = strtod(bncRateCache[symbol].c_str(), nullptr);

    if (rateValue != 0.0)
        rate = (bncGlobalPrice * (1.0 / rateValue));

    ABC_DebugLog("Exchange Response: %s = %f\n", symbol, rate);
exit:

    return cc;
}


tABC_CC ABC_ExchangeBitStampRate(int currencyNum, double &rate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t error;
    json_t *pJSON_Root = NULL;
    std::string reply;

    // Fetch exchanges from bitstamp
    ABC_CHECK_RET(ABC_ExchangeGet(BITSTAMP_RATE_URL, reply, pError));

    // Parse the json
    pJSON_Root = json_loads(reply.c_str(), 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // USD
    ABC_ExchangeExtract(pJSON_Root, "last", CURRENCY_NUM_USD, rate, pError);
exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    return cc;
}

tABC_CC ABC_ExchangeCoinBaseRates(int currencyNum, double &rate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t error;
    json_t *pJSON_Root = NULL;
    std::string reply;
    std::string field;

    // Fetch exchanges from coinbase
    ABC_CHECK_RET(ABC_ExchangeGet(COINBASE_RATE_URL, reply, pError));

    // Parse the json
    pJSON_Root = json_loads(reply.c_str(), 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    ABC_CHECK_RET(ABC_ExchangeCoinBaseMap(currencyNum, field, pError));
    ABC_ExchangeExtract(pJSON_Root, field.c_str(), currencyNum, rate, pError);
exit:
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

tABC_CC ABC_ExchangeBncRates(int currencyNum, double &rate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    switch (currencyNum) {
        case CURRENCY_NUM_USD:
            ABC_ExchangeBncGlobalPrice("USD", rate, pError);
            break;
        case CURRENCY_NUM_AUD:
            ABC_ExchangeBncGlobalPrice("AUD", rate, pError);
            break;
        case CURRENCY_NUM_CAD:
            ABC_ExchangeBncGlobalPrice("CAD", rate, pError);
            break;
        case CURRENCY_NUM_CNY:
            ABC_ExchangeBncGlobalPrice("CNY", rate, pError);
            break;
        case CURRENCY_NUM_HKD:
            ABC_ExchangeBncGlobalPrice("HKD", rate, pError);
            break;
        case CURRENCY_NUM_MXN:
            ABC_ExchangeBncGlobalPrice("MXN", rate, pError);
            break;
        case CURRENCY_NUM_NZD:
            ABC_ExchangeBncGlobalPrice("NZD", rate, pError);
            break;
        case CURRENCY_NUM_GBP:
            ABC_ExchangeBncGlobalPrice("GBP", rate, pError);
            break;
        case CURRENCY_NUM_EUR:
            ABC_ExchangeBncGlobalPrice("EUR", rate, pError);
            break;
        default:
            ABC_CHECK_ASSERT(false, ABC_CC_Error, "Unsupported currency");
    }
exit:
    return cc;
}

} // namespace abcd
