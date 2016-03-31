/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "AddressCache.hpp"
#include <algorithm>

namespace abcd {

constexpr std::chrono::seconds periodDefault(20);
constexpr std::chrono::seconds periodPriority(4);

void
AddressCache::insert(const std::string &address)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (rows_.end() == rows_.find(address))
    {
        rows_[address] = AddressRow
        {
            periodDefault,
            std::chrono::steady_clock::now() - periodDefault,
            false, false
        };

        if (wakeupCallback_)
            wakeupCallback_();
    }
}

void
AddressCache::prioritize(const std::string &address)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!priorityAddress_.empty())
        rows_[priorityAddress_].period = periodDefault;

    priorityAddress_ = address;
    if (!priorityAddress_.empty())
        rows_[priorityAddress_].period = periodPriority;

    if (wakeupCallback_)
        wakeupCallback_();
}

std::chrono::milliseconds
AddressCache::nextWakeup(std::string &nextAddress)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const auto now = std::chrono::steady_clock::now();
    auto maxLag = std::chrono::steady_clock::duration::zero();
    auto minWait = std::chrono::steady_clock::duration::zero();
    bool firstLoop = true;

    for (const auto &row: rows_)
    {
        // Ignore addresses that are already being checked:
        if (!row.second.checking)
        {
            auto nextCheck = row.second.lastCheck + row.second.period;
            if (nextCheck <= now)
            {
                // The time to check is now:
                auto lag = now - nextCheck;
                if (maxLag <= lag)
                {
                    maxLag = lag;
                    nextAddress = row.first;
                    minWait = minWait.zero();
                }
            }
            else
            {
                // The check is in the future:
                auto wait = nextCheck - now;
                if (wait < minWait || firstLoop)
                    minWait = wait;
            }
            firstLoop = false;
        }
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(minWait);
}

void
AddressCache::checkBegin(const std::string &address)
{
    std::lock_guard<std::mutex> lock(mutex_);

    rows_[address].checking = true;
}

void
AddressCache::checkEnd(const std::string &address, bool success)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto &row = rows_[address];
    row.checking = false;
    if (success)
    {
        row.lastCheck = std::chrono::steady_clock::now();
        row.checkedOnce = true;

        if (doneCallback_ && done())
        {
            doneCallback_();
            doneCallback_ = nullptr;
        }
    }
}

void
AddressCache::doneCallbackSet(const Callback &callback)
{
    std::lock_guard<std::mutex> lock(mutex_);

    doneCallback_ = callback;
    if (doneCallback_ && done())
    {
        doneCallback_();
        doneCallback_ = nullptr;
    }
}

void
AddressCache::wakeupCallbackSet(const Callback &callback)
{
    std::lock_guard<std::mutex> lock(mutex_);
    wakeupCallback_ = callback;
}

bool
AddressCache::done()
{
    bool allChecked = true;
    for (const auto &row: rows_)
    {
        if (!row.second.checkedOnce)
        {
            allChecked = false;
            break;
        }
    }
    return allChecked;
}

} // namespace abcd
