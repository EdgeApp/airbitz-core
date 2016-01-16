/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_EXCHANGE_EXCHANGE_CACHE_H
#define ABCD_EXCHANGE_EXCHANGE_CACHE_H

#include "Currency.hpp"
#include "ExchangeSource.hpp"
#include <time.h>
#include <map>
#include <mutex>

namespace abcd {

/**
 * A cache for Bitcoin rates.
 */
class ExchangeCache
{
public:
    ExchangeCache(const std::string &path);

    /**
     * Updates the exchange rates, trying the sources in the given order.
     */
    Status
    update(Currencies currencies, const ExchangeSources &sources);

    Status
    satoshiToCurrency(double &result, int64_t in, Currency currency);

    Status
    currencyToSatoshi(int64_t &result, double in, Currency currency);

private:
    mutable std::mutex mutex_;
    const std::string path_;

    struct CacheRow
    {
        double rate;
        time_t timestamp;
    };
    std::map<Currency, CacheRow> cache_;

    /**
     * Loads the cache from disk.
     */
    Status
    load();

    /**
     * Flushes the cache to disk.
     */
    Status
    save();

    /**
     * Obtains a rate from the cache.
     */
    Status
    rate(double &result, Currency currency);

    /**
     * Adds a rate to the cache.
     */
    Status
    update(Currency currency, double rate, time_t now);

    /**
     * Returns true if all the listed rates are fresh in the cache.
     */
    bool
    fresh(const Currencies &currencies, time_t now);
};

} // namespace abcd

#endif
