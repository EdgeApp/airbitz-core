/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "ExchangeSource.hpp"
#include "../json/JsonArray.hpp"
#include "../json/JsonObject.hpp"
#include "../util/URL.hpp"
#include <curl/curl.h>
#include <stdlib.h>

namespace abcd {

struct BitstampJson:
    public JsonObject
{
    ABC_JSON_STRING(rate, "last", nullptr)
};

struct BraveNewCoinJson:
    public JsonObject
{
    ABC_JSON_VALUE(rates, "rates", JsonArray)
};

struct BraveNewCoinJsonRow:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(BraveNewCoinJsonRow, JsonObject)
    ABC_JSON_STRING(code,   "id_currency",  nullptr)
    ABC_JSON_STRING(rate,   "rate",         nullptr)
    ABC_JSON_STRING(crypto, "crypto",       "1")
};

const ExchangeSources exchangeSources
{
    "BraveNewCoin", "Coinbase", "Bitstamp"
};

/**
 * Helper for exchangeSourceCurlGet.
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
 * Does an HTTP GET request.
 */
static Status
exchangeSourceCurlGet(std::string &result, const char *url)
{
    std::string out;
    AutoFree<CURL, curl_easy_cleanup> curlHandle;
    ABC_CHECK_OLD(ABC_URLCurlHandleInit(&curlHandle.get(), &error));

    // Set options:
    if (curl_easy_setopt(curlHandle, CURLOPT_SSL_VERIFYPEER, 1L))
        return ABC_ERROR(ABC_CC_Error, "Unable to verify servers cert");
    if (curl_easy_setopt(curlHandle, CURLOPT_URL, url))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set URL");
    if (curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &out))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set data");
    if (curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, curlWriteData))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to set callback");

    // Do the GET:
    if (curl_easy_perform(curlHandle))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to perform");

    // Check the result:
    long resCode;
    if (curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &resCode))
        return ABC_ERROR(ABC_CC_Error, "Curl failed to retrieve response info");
    if (resCode < 200 || 300 <= resCode)
        return ABC_ERROR(ABC_CC_Error, "Bad HTTP response code");

    result = std::move(out);
    return Status();
}

static Status
doubleDecode(double &result, const char *in)
{
    char *endptr;
    double out = strtod(in, &endptr);
    if (*endptr)
        return ABC_ERROR(ABC_CC_ParseError, "Malformed decimal number");

    result = out;
    return Status();
}

/**
 * Fetches exchange rates from the Bitstamp source.
 */
static Status
fetchBitstamp(ExchangeRates &result)
{
    std::string raw;
    ABC_CHECK(exchangeSourceCurlGet(raw, "https://www.bitstamp.net/api/ticker/"));

    BitstampJson json;
    ABC_CHECK(json.decode(raw));
    ABC_CHECK(json.rateOk());

    double rate;
    ABC_CHECK(doubleDecode(rate, json.rate()));
    ExchangeRates out;
    out[Currency::USD] = rate;

    result = std::move(out);
    return Status();
}

/**
 * Fetches and decodes exchange rates from the BraveNewCoin source.
 */
static Status
fetchBraveNewCoin(ExchangeRates &result)
{
    std::string raw;
    ABC_CHECK(exchangeSourceCurlGet(raw, "http://api.bravenewcoin.com/rates.json"));

    BraveNewCoinJson json;
    ABC_CHECK(json.decode(raw));
    auto rates = json.rates();

    // Break apart the array:
    ExchangeRates out;
    double btcRate = 0;
    auto size = rates.size();
    for (size_t i = 0; i < size; ++i)
    {
        BraveNewCoinJsonRow row(rates[i]);
        ABC_CHECK(row.codeOk());
        ABC_CHECK(row.rateOk());

        // Capture the special BTC rate:
        if (!strcmp(row.code(), "BTC"))
            ABC_CHECK(doubleDecode(btcRate, row.rate()));

        // Skip cryptos and unknown currencies:
        Currency currency;
        if (!strcmp(row.crypto(), "1"))
            continue;
        if (!currencyNumber(currency, row.code()))
            continue;

        // Capture the value:
        double rate;
        ABC_CHECK(doubleDecode(rate, row.rate()));
        out[currency] = rate;
    }

    // Adjust the currencies by the BTC rate:
    if (0 == btcRate)
        return ABC_ERROR(ABC_CC_Error, "No BTC rate from BraveNewCoin");
    for (auto &i: out)
        i.second = btcRate / i.second;

    result = std::move(out);
    return Status();
}

/**
 * Fetches and decodes exchange rates from the Coinbase source.
 */
static Status
fetchCoinbase(ExchangeRates &result)
{
    std::string raw;
    ABC_CHECK(exchangeSourceCurlGet(raw, "https://coinbase.com/api/v1/currencies/exchange_rates"));

    JsonObject json;
    ABC_CHECK(json.decode(raw));

    // Check for usable rates:
    ExchangeRates out;
    for (void *i = json_object_iter(json.get());
        i;
        i = json_object_iter_next(json.get(), i))
    {
        // Extract the three-letter currency code:
        const char *key = json_object_iter_key(i);
        if (strncmp(key, "btc_to_", 7))
            continue;
        std::string code = key + 7;

        // Try to look up the code:
        Currency currency;
        for (auto &c: code)
            c = toupper(c);
        if (!currencyNumber(currency, code))
            continue;

        // Capture the value:
        const char *value = json_string_value(json_object_iter_value(i));
        if (!value)
            return ABC_ERROR(ABC_CC_JSONError, "Bad Coinbase rate string.");

        double rate;
        ABC_CHECK(doubleDecode(rate, value));
        out[currency] = rate;
    }

    result = std::move(out);
    return Status();
}

Status
exchangeSourceFetch(ExchangeRates &result, const std::string &source)
{
    if (source == "Bitstamp")       return fetchBitstamp(result);
    if (source == "BraveNewCoin")   return fetchBraveNewCoin(result);
    if (source == "Coinbase")       return fetchCoinbase(result);
    return ABC_ERROR(ABC_CC_ParseError, "No exchange-rate source " + source);
}

} // namespace abcd
