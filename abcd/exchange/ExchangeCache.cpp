/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "ExchangeCache.hpp"
#include "../util/Util.hpp"
#include <map>
#include <mutex>

namespace abcd {

struct ExchangeCache
{
    double rate;
    time_t lastUpdate;
};
std::map<int, ExchangeCache> gExchangeCache;
std::mutex gExchangeMutex;

bool
exchangeCacheGet(int currencyNum, double &rate, time_t &lastUpdate)
{
    std::lock_guard<std::mutex> lock(gExchangeMutex);

    auto i = gExchangeCache.find(currencyNum);
    if (i != gExchangeCache.end())
    {
        rate = i->second.rate;
        lastUpdate = i->second.lastUpdate;
        return true;
    }
    return false;
}

Status
exchangeCacheSet(int currencyNum, double rate)
{
    std::lock_guard<std::mutex> lock(gExchangeMutex);

    gExchangeCache[currencyNum] = ExchangeCache{rate, time(nullptr)};

    return Status();
}

} // namespace abcd
