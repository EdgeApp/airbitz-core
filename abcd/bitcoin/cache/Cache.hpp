/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_CACHE_CACHE_HPP
#define ABCD_BITCOIN_CACHE_CACHE_HPP

#include "AddressCache.hpp"
#include "TxCache.hpp"

namespace abcd {

class Cache
{
public:
    TxCache txs;
    AddressCache addresses;

    Cache(const std::string &path);

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
     * Saves the cache to disk.
     */
    Status
    save();

private:
    const std::string path_;
};

} // namespace abcd

#endif
