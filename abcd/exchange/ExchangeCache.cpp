/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "ExchangeCache.hpp"
#include "../json/JsonArray.hpp"
#include "../json/JsonObject.hpp"

namespace abcd {

#define SATOSHI_PER_BITCOIN 100000000

struct CacheJson:
    public JsonObject
{
    ABC_JSON_VALUE(rates, "rates", JsonArray)
};

struct CacheJsonRow:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(CacheJsonRow, JsonObject)
    ABC_JSON_STRING(code, "code", nullptr)
    ABC_JSON_NUMBER(rate, "rate", 1)
    ABC_JSON_INTEGER(timestamp, "timestamp", 0)
};

ExchangeCache::ExchangeCache(const std::string &dir):
    path_(dir + "Exchange.json")
{
    load(); // Nothing bad happens if this fails
}

Status
ExchangeCache::update(Currencies currencies, const ExchangeSources &sources)
{
    // No mutex, since we are making network calls.
    // We only call member functions that provide their own mutexes.

    time_t now = time(nullptr);
    if (fresh(currencies, now))
        return Status();

    ExchangeRates allRates;
    for (auto source: sources)
    {
        // Stop if the todo list is empty:
        if (currencies.empty())
            break;

        // Grab the rates from the server:
        ExchangeRates rates;
        if (!exchangeSourceFetch(rates, source))
            continue; // Just skip the failed ones

        // Remove any new rates from the todo list:
        for (auto rate: rates)
        {
            auto i = currencies.find(rate.first);
            if (currencies.end() != i)
                currencies.erase(i);
        }

        // Add any new rates to the allRates list:
        rates.insert(allRates.begin(), allRates.end());
        allRates = std::move(rates);
    }

    // Add the rates to the cache:
    for (auto rate: allRates)
        ABC_CHECK(update(rate.first, rate.second, now));
    ABC_CHECK(save());

    return Status();
}

Status
ExchangeCache::satoshiToCurrency(double &result, int64_t in, Currency currency)
{
    result = 0.0;

    double r;
    ABC_CHECK(rate(r, currency));

    result = in * (r / SATOSHI_PER_BITCOIN);
    return Status();
}

Status
ExchangeCache::currencyToSatoshi(int64_t &result, double in, Currency currency)
{
    result = 0;

    double r;
    ABC_CHECK(rate(r, currency));

    result = static_cast<int64_t>(in * (SATOSHI_PER_BITCOIN / r));
    return Status();
}

Status
ExchangeCache::load()
{
    std::lock_guard<std::mutex> lock(mutex_);

    CacheJson json;
    ABC_CHECK(json.load(path_));

    auto arrayJson = json.rates();
    auto size = arrayJson.size();
    for (size_t i = 0; i < size; i++)
    {
        CacheJsonRow row(arrayJson[i]);
        ABC_CHECK(row.codeOk());
        ABC_CHECK(row.rateOk());
        ABC_CHECK(row.timestampOk());

        Currency currency;
        ABC_CHECK(currencyNumber(currency, row.code()));
        cache_[currency] =
            CacheRow{row.rate(), static_cast<time_t>(row.timestamp())};
    }

    return Status();
}

Status
ExchangeCache::save()
{
    std::lock_guard<std::mutex> lock(mutex_);

    JsonArray rates;
    for (const auto &i: cache_)
    {
        std::string code;
        ABC_CHECK(currencyCode(code, i.first));

        CacheJsonRow row;
        ABC_CHECK(row.codeSet(code));
        ABC_CHECK(row.rateSet(i.second.rate));
        ABC_CHECK(row.timestampSet(i.second.timestamp));
        ABC_CHECK(rates.append(row));
    }

    CacheJson json;
    ABC_CHECK(json.ratesSet(rates));
    ABC_CHECK(json.save(path_));

    return Status();
}

Status
ExchangeCache::rate(double &result, Currency currency)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const auto &i = cache_.find(currency);
    if (cache_.end() == i)
        return ABC_ERROR(ABC_CC_Error, "Currency not in cache");

    result = i->second.rate;
    return Status();
}

Status
ExchangeCache::update(Currency currency, double rate, time_t now)
{
    std::lock_guard<std::mutex> lock(mutex_);

    cache_[currency] = CacheRow{rate, now};
    return Status();
}

bool
ExchangeCache::fresh(const Currencies &currencies, time_t now)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto currency: currencies)
    {
        auto i = cache_.find(currency);
        if (cache_.end() == i)
            return false;
        if (i->second.timestamp + ABC_EXCHANGE_RATE_REFRESH_INTERVAL_SECONDS < now)
            return false;
    }
    return true;
}

} // namespace abcd
