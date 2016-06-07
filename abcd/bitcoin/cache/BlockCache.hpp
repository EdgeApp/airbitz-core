/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_BLOCK_CACHE_HPP
#define ABCD_BITCOIN_BLOCK_CACHE_HPP

#include "../../util/Status.hpp"
#include <functional>
#include <mutex>

namespace abcd {

/**
 * A block-height cache.
 */
class BlockCache
{
public:
    typedef std::function<void (size_t height)> HeightCallback;

    // Lifetime -----------------------------------------------------------

    BlockCache(const std::string &path);

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

    // Queries ------------------------------------------------------------

    /**
     * Returns the highest block that this cache has seen.
     */
    size_t
    height() const;

    // Updates ------------------------------------------------------------

    /**
     * Updates the block height.
     */
    void
    heightSet(size_t height);

    /**
     * Provides a callback to be invoked when the chain height changes.
     */
    void
    onHeightSet(const HeightCallback &onHeight);

private:
    mutable std::mutex mutex_;
    const std::string path_;
    bool dirty_;

    // Chain height:
    size_t height_;
    HeightCallback onHeight_;
};

} // namespace abcd

#endif
