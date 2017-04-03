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

typedef enum
{
    ServerTypeStratum,
    ServerTypeLibbitcoin
} ServerType;

typedef struct
{
    std::string serverUrl;
    int score;
    unsigned long responseTime;
    unsigned long numResponseTimes;
} ServerInfo;

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
    serverCacheLoad();

    /**
     * Saves the database contents to disk, but only if there are changes.
     */
    Status
    serverCacheSave();

    /**
     * Increase server score
     */
    Status
    serverScoreUp(std::string serverUrl, int changeScore=1);

    /**
     * Decrease server score
     */
    Status
    serverScoreDown(std::string serverUrl, int changeScore=10);

    /**
     * Set the response time seen from an interaction with this server
     */
    void
    setResponseTime(std::string serverUrl,
                    unsigned long long responseTimeMilliseconds);

    /**
     * Get a vector of server URLs by type. This returns the top 'numServers' of servers with
     * the highest connectivity score
     */
    std::vector<std::string>
    getServers(ServerType type, unsigned int numServers);

    static
    unsigned long long getCurrentTimeMilliSeconds();

private:
    Status
    save_nolock();

    mutable std::mutex mutex_;
    const std::string path_;
    bool dirty_;
    time_t lastUpScoreTime_;
    time_t cacheLastSave_;

    std::map<std::string, ServerInfo> servers_;
};

} // namespace abcd

#endif
