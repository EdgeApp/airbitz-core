/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * AirBitz in-memory exchange-rate caching system.
 */

#ifndef ABCD_EXCHANGE_EXCHANGE_CACHE_H
#define ABCD_EXCHANGE_EXCHANGE_CACHE_H

#include "../util/Status.hpp"
#include <time.h>

namespace abcd {

/**
 * Retrieves an entry from the in-memory cache.
 * @return false if the entry is not available.
 */
bool
exchangeCacheGet(int currencyNum, double &rate, time_t &lastUpdate);

/**
 * Saves an entry in the in-memory cache.
 */
Status
exchangeCacheSet(int currencyNum, double rate);

} // namespace abcd

#endif
