/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_BLOCK_CACHE_HPP
#define ABCD_BITCOIN_BLOCK_CACHE_HPP

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
class BlockCache
{
public:
    typedef std::function<void (size_t height)> HeightCallback;
    typedef std::function<void (void)> HeaderCallback;

    // Lifetime ------------------------------------------------------------

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

    // Chain height --------------------------------------------------------

    /**
     * Returns the highest block that this cache has seen.
     */
    size_t
    height() const;

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

    // Block headers -------------------------------------------------------

    /**
     * Retrieves a header's timestamp from the cache.
     */
    Status
    headerTime(time_t &result, size_t height);

    /**
     * Stores a block header in the cache.
     */
    bool
    headerInsert(size_t height, const libbitcoin::block_header_type &header);

    /**
     * Provides a callback to be invoked when a new header is inserted.
     */
    void
    onHeaderSet(const HeaderCallback &onHeader);

    /**
     * Invokes the `onHeader` callback, but only if there are new headers,
     * and enough time has elapsed.
     */
    void
    onHeaderInvoke(void);

    // Missing header list -------------------------------------------------

    /**
     * Returns the next requested block header missing from the cache,
     * or zero if there is none.
     */
    size_t
    headerNeeded();

    /**
     * Requests that a particular block header be added to the cache.
     */
    void
    headerNeededAdd(size_t height);

private:
    mutable std::mutex mutex_;
    const std::string path_;
    bool dirty_;

    // Chain height:
    size_t height_;
    HeightCallback onHeight_;

    // Chain headers:
    std::map<size_t, libbitcoin::block_header_type> headers_;
    bool headersDirty_ = false;
    time_t onHeaderLastCall_ = 0;
    HeaderCallback onHeader_;

    // Missing headers:
    std::set<size_t> headersNeeded_;
};

} // namespace abcd

#endif
