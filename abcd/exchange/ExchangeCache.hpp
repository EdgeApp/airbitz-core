/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_EXCHANGE_EXCHANGE_CACHE_H
#define ABCD_EXCHANGE_EXCHANGE_CACHE_H

#include "../util/Status.hpp"
#include <time.h>
#include <map>

namespace abcd {

enum class Currency;

/**
 * A cache for Bitcoin rates.
 */
class ExchangeCache
{
public:
    ExchangeCache(const std::string &dir);

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
     * Returns true if the rate is fresh in the cache.
     */
    bool
    fresh(Currency currency, time_t now);

private:
    const std::string dir_;

    struct CacheRow
    {
        double rate;
        time_t timestamp;
    };
    std::map<Currency, CacheRow> cache_;
};

} // namespace abcd

#endif
