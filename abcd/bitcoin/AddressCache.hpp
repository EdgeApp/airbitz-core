/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_ADDRESS_CACHE_HPP
#define ABCD_BITCOIN_ADDRESS_CACHE_HPP

#include "../util/Status.hpp"
#include <chrono>
#include <map>
#include <mutex>

namespace abcd {

/**
 * Tracks address query freshness.
 *
 * The long-term plan is to make this class work with the transaction cache.
 * It should be able to pick good poll frequencies for each address,
 * and should also generate new addresses based on the HD gap limit.
 * This class should also cache its contents on disk,
 * avoiding the need to re-check everything on each login.
 *
 * This will allow the `AddressDb` to be a simple metadata store,
 * with no need to handle Bitcoin-specific knowledge.
 */
class AddressCache
{
public:
    typedef std::function<void()> Callback;

    /**
     * Begins watching an address.
     */
    void
    insert(const std::string &address);

    /**
     * Begins checking the provided address at high speed.
     * Pass a blank address to cancel the priority polling.
     */
    void
    prioritize(const std::string &address);

    /**
     * Returns the amount of time until the next wakeup needs to happen.
     * If there are stale addresses, the wakeup would be in the past.
     * In that case, this function returns 0 and sets `nextAddress`
     * to the most out-of-date address.
     * Also returns 0 if there are no active addresses at all.
     */
    std::chrono::milliseconds
    nextWakeup(std::string &nextAddress);

    /**
     * Indicates that a watcher is currently checking this address.
     */
    void
    checkBegin(const std::string &address);

    /**
     * Indicates that a watcher has finished checking this address.
     * @param success true to indicate that the address is now up to date,
     * or false to indicate that the check failed.
     */
    void
    checkEnd(const std::string &address, bool success);

    /**
     * Sets up a callback to notify when addresses change.
     */
    void
    wakeupCallbackSet(const Callback &callback);

private:
    mutable std::mutex mutex_;
    std::string priorityAddress_;

    struct AddressRow
    {
        std::chrono::milliseconds period;
        std::chrono::steady_clock::time_point lastCheck;
        bool checking;
        bool checkedOnce;
    };
    std::map<std::string, AddressRow> rows_;

    Callback wakeupCallback_;
};

} // namespace abcd

#endif
