/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "ExchangeSource.hpp"
#include "../http/HttpRequest.hpp"
#include "../json/JsonArray.hpp"
#include "../json/JsonObject.hpp"
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

struct CleverCoinJson:
    public JsonObject
{
    ABC_JSON_STRING(rate, "last", nullptr)
};

const ExchangeSources exchangeSources
{
    "Bitstamp", "BraveNewCoin", "Coinbase", "CleverCoin"
};

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
    HttpReply reply;
    ABC_CHECK(HttpRequest().get(reply, "https://www.bitstamp.net/api/ticker/"));
    ABC_CHECK(reply.codeOk());

    BitstampJson json;
    ABC_CHECK(json.decode(reply.body));
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
    HttpReply reply;
    ABC_CHECK(HttpRequest().get(reply, "http://api.bravenewcoin.com/rates.json"));
    ABC_CHECK(reply.codeOk());

    BraveNewCoinJson json;
    ABC_CHECK(json.decode(reply.body));
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
    HttpReply reply;
    ABC_CHECK(HttpRequest().get(reply,
                                "https://coinbase.com/api/v1/currencies/exchange_rates"));
    ABC_CHECK(reply.codeOk());

    JsonObject json;
    ABC_CHECK(json.decode(reply.body));

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

/**
 * Fetches exchange rates from the CleverCoin source.
 */
static Status
fetchCleverCoin(ExchangeRates &result)
{
    HttpReply reply;
    ABC_CHECK(HttpRequest().get(reply, "https://api.clevercoin.com/v1/ticker"));
    ABC_CHECK(reply.codeOk());

    CleverCoinJson json;
    ABC_CHECK(json.decode(reply.body));
    ABC_CHECK(json.rateOk());

    double rate;
    ABC_CHECK(doubleDecode(rate, json.rate()));
    ExchangeRates out;
    out[Currency::EUR] = rate;

    result = std::move(out);
    return Status();
}

Status
exchangeSourceFetch(ExchangeRates &result, const std::string &source)
{
    if (source == "Bitstamp")       return fetchBitstamp(result);
    if (source == "BraveNewCoin")   return fetchBraveNewCoin(result);
    if (source == "Coinbase")       return fetchCoinbase(result);
    if (source == "CleverCoin")     return fetchCleverCoin(result);
    return ABC_ERROR(ABC_CC_ParseError, "No exchange-rate source " + source);
}

} // namespace abcd
