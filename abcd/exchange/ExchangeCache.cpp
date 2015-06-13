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

#define FILENAME "Exchange.json"

static std::mutex exchangeCacheMutex;

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
    dir_(dir)
{}

Status
ExchangeCache::load()
{
    CacheJson json;
    std::lock_guard<std::mutex> lock(exchangeCacheMutex);

    ABC_CHECK(json.load(dir_ + FILENAME));
    auto rates = json.rates();

    auto size = rates.size();
    for (size_t i = 0; i < size; i++)
    {
        CacheJsonRow row(rates[i]);
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
    JsonArray rates;
    std::lock_guard<std::mutex> lock(exchangeCacheMutex);
    for (const auto &i: cache_)
    {
        std::string code;
        ABC_CHECK(currencyCode(code, i.first));

        CacheJsonRow row;
        ABC_CHECK(row.codeSet(code.c_str()));
        ABC_CHECK(row.rateSet(i.second.rate));
        ABC_CHECK(row.timestampSet(i.second.timestamp));
        ABC_CHECK(rates.append(row));
    }

    CacheJson json;
    ABC_CHECK(json.ratesSet(rates));
    ABC_CHECK(json.save(dir_ + FILENAME));

    return Status();
}

Status
ExchangeCache::rate(double &result, Currency currency)
{
    std::lock_guard<std::mutex> lock(exchangeCacheMutex);
    const auto &i = cache_.find(currency);
    if (cache_.end() == i)
        return ABC_ERROR(ABC_CC_Error, "Currency not in cache");

    result = i->second.rate;
    return Status();
}

Status
ExchangeCache::update(Currency currency, double rate, time_t now)
{
    std::lock_guard<std::mutex> lock(exchangeCacheMutex);
    cache_[currency] = CacheRow{rate, now};
    return Status();
}

bool
ExchangeCache::fresh(const Currencies &currencies, time_t now)
{
    std::lock_guard<std::mutex> lock(exchangeCacheMutex);
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
