/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "ServerCache.hpp"
#include "../Utility.hpp"
#include "../../crypto/Encoding.hpp"
#include "../../json/JsonArray.hpp"
#include "../../json/JsonObject.hpp"
#include "../../util/Debug.hpp"
#include "../../General.hpp"


namespace abcd {

constexpr auto LIBBITCOIN_PREFIX = "tcp://";
constexpr auto STRATUM_PREFIX = "stratum://";
constexpr auto LIBBITCOIN_PREFIX_LENGTH = 6;
constexpr auto STRATUM_PREFIX_LENGTH = 10;
constexpr auto MAX_SCORE = 100;
constexpr auto MIN_SCORE = -100;

constexpr time_t onHeaderTimeout = 5;

struct ServerScoreJson:
        public JsonObject
{
    ABC_JSON_CONSTRUCTORS(ServerScoreJson, JsonObject)

    ABC_JSON_STRING(serverUrl, "serverUrl", "")
    ABC_JSON_INTEGER(serverScore, "serverScore", 0)
};



ServerCache::ServerCache(const std::string &path):
    path_(path),
    dirty_(false),
    lastUpScoreTime_(0)
{
}

void
ServerCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    servers_.clear();
}

Status
ServerCache::load()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ABC_Debug(1, "ServerCache::load");

    // Load the saved server scores if they exist
    JsonArray serverScoresJsonArray;

    // It's ok if this fails
    serverScoresJsonArray.load(path_).log();

    // Add any new servers coming out of the auth server
    std::vector<std::string> bitcoinServers = generalBitcoinServers();

    size_t size = bitcoinServers.size();
    for (size_t i = 0; i < size; i++)
    {
        std::string serverUrlNew = bitcoinServers[i];

        size_t sizeScores = serverScoresJsonArray.size();
        bool serversMatch = false;
        for (size_t j = 0; j < sizeScores; j++)
        {
            ServerScoreJson ssj = serverScoresJsonArray[j];
            std::string serverUrl(ssj.serverUrl());

            if (boost::equal(serverUrl, serverUrlNew))
            {
                serversMatch = true;
                break;
            }
        }
        if (!serversMatch)
        {
            // Found a new server. Add it
            ServerScoreJson ssjNew;
            ssjNew.serverUrlSet(serverUrlNew);
            ssjNew.serverScoreSet(0);
            serverScoresJsonArray.append(ssjNew);
            dirty_ = true;
        }
    }

    // Load the servers into the servers_ map
    servers_.clear();

    size_t numServers = serverScoresJsonArray.size();
    for (size_t j = 0; j < numServers; j++)
    {
        ServerScoreJson ssj = serverScoresJsonArray[j];
        std::string serverUrl = ssj.serverUrl();
        int serverScore = ssj.serverScore();
        servers_[serverUrl] = serverScore;
    }

    return save_nolock();
}

Status
ServerCache::save_nolock()
{
    JsonArray serverScoresJsonArray;
    ABC_Debug(1, "ServerCache::save");

    if (dirty_)
    {
        for (const auto &server: servers_)
        {
            ServerScoreJson ssj;
            ABC_CHECK(ssj.serverUrlSet(server.first));
            ABC_CHECK(ssj.serverScoreSet(server.second));
            ABC_CHECK(serverScoresJsonArray.append(ssj));
        }
        ABC_CHECK(serverScoresJsonArray.save(path_));
        dirty_ = false;
    }

    return Status();
}

Status
ServerCache::save()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return save_nolock();
}

Status
ServerCache::serverScoreUp(std::string serverUrl, int changeScore)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto svr = servers_.find(serverUrl);
    if (servers_.end() != svr)
    {
        int score = svr->second;
        score += changeScore;
        if (score > MAX_SCORE)
            score = MAX_SCORE;
        servers_[serverUrl] = score;
        dirty_ = true;
        ABC_Debug(1, "serverScoreUp:" + serverUrl + " " + std::to_string(score));
    }
    lastUpScoreTime_ = time(nullptr);
    return Status();
}

Status
ServerCache::serverScoreDown(std::string serverUrl, int changeScore)
{
    std::lock_guard<std::mutex> lock(mutex_);

    time_t currentTime = time(nullptr);

    if (currentTime - lastUpScoreTime_ > 60)
    {
        // It has been over 1 minute since we got an upvote for any server.
        // Assume the network is down and don't penalize anyone for now
        return Status();
    }

    auto svr = servers_.find(serverUrl);
    if (servers_.end() != svr)
    {
        int score = svr->second;
        score -= changeScore;
        if (score < MIN_SCORE)
            score = MIN_SCORE;
        servers_[serverUrl] = score;
        dirty_ = true;
        ABC_Debug(1, "serverScoreDown:" + serverUrl + " " + std::to_string(score));
    }
    return Status();
}

std::vector<std::string>
ServerCache::getServers(ServerType type, unsigned int numServers)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> servers;

    // Get the top [numServers] with the highest score sorted in order of score
    for (int i = MAX_SCORE; i >= MIN_SCORE; i--)
    {
        for (const auto &server: servers_)
        {
            if (ServerTypeStratum == type)
            {
                if (0 != server.first.compare(0, STRATUM_PREFIX_LENGTH, STRATUM_PREFIX))
                    continue;
            }
            else if (ServerTypeLibbitcoin == type)
            {
                if (0 != server.first.compare(0, LIBBITCOIN_PREFIX_LENGTH, LIBBITCOIN_PREFIX))
                    continue;
            }

            if (server.second == i)
            {
                ABC_Debug(1, "ServerCache::getServers [" + std::to_string(i) + "]:" + server.first );
                servers.push_back(server.first);
                numServers--;
                if (numServers == 0)
                    return servers;
            }
        }
    }
    return servers;
}
} // namespace abcd
