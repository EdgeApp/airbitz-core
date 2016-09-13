/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_CACHE_CACHE_HPP
#define ABCD_BITCOIN_CACHE_CACHE_HPP

#include "AddressCache.hpp"
#include "BlockCache.hpp"
#include "TxCache.hpp"
#include "ServerCache.hpp"

namespace abcd {

class Cache
{
public:
    TxCache txs;
    BlockCache &blocks;
    AddressCache addresses;
    ServerCache serverCache;

    Cache(const std::string &path, BlockCache &blockCache);

    /**
     * Sets the address check done for this wallet meaning that
     * this wallet has had all addresses checked at least once on
     * this device
     */
    void
    addressCheckDoneSet();

    /**
     * Gets the address check done for this wallet meaning that
     * this wallet has had all addresses checked at least once on
     * this device
     */
    bool
    addressCheckDoneGet();

    /**
     * Clears the cache in case something goes wrong.
     */
    void
    clear();

    /**
     * Loads the cache from disk.
     */
    Status
    load();

    /**
     * Loads the cache from the legacy format.
     */
    Status
    loadLegacy(const std::string &path);

    /**
     * Saves the cache to disk.
     */
    Status
    save();

private:

    /**
     * Save the status of addressCheckDone in the cache
     */
    Status
    addressCheckDoneSave(JsonObject &json);

    /**
     * Load the status of addressCheckDone from the cache
     */
    void
    addressCheckDoneLoad(JsonObject &json);

    const std::string path_;
    bool addressCheckDone_;
};

} // namespace abcd

#endif
