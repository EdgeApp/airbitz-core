/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_SERVER_CACHE_HPP
#define ABCD_BITCOIN_SERVER_CACHE_HPP

#include "../../util/Status.hpp"
#include <bitcoin/bitcoin.hpp>
#include <functional>
#include <map>
#include <mutex>
#include <set>

namespace abcd {

/**
 * A block-height cache.
 */
class ServerCache
{
public:

    // Lifetime ------------------------------------------------------------

    ServerCache(const std::string &path);

    /**
     * Clears the cache in case something goes wrong.
     */
    void
    clear();

    /**
     * Reads the database contents from disk.
     */
    Status
    load();

    /**
     * Saves the database contents to disk, but only if there are changes.
     */
    Status
    save();

private:
    mutable std::mutex mutex_;
    const std::string path_;
    bool dirty_;

    std::map<std::string, int> servers_;
};

} // namespace abcd

#endif
