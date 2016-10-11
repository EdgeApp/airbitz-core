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
constexpr auto MAX_SCORE = 500;
constexpr auto MIN_SCORE = -100;

#define RESPONSE_TIME_UNINITIALIZED 999999999

/**
 * Utility routines
 */
bool sortServersByTime(ServerInfo si1, ServerInfo si2)
{
    return si1.responseTime < si2.responseTime;
}

bool sortServersByScore(ServerInfo si1, ServerInfo si2)
{
    return si1.score > si2.score;
}

int roundUpDivide(int x, int y)
{
    return (x % y) ? (x / y + 1) : (x / y);
}


struct ServerScoreJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(ServerScoreJson, JsonObject)

    ABC_JSON_STRING(serverUrl, "serverUrl", "")
    ABC_JSON_INTEGER(serverScore, "serverScore", 0)
    ABC_JSON_INTEGER(serverResponseTime, "serverResponseTime",
                     RESPONSE_TIME_UNINITIALIZED)
};

ServerCache::ServerCache(const std::string &path):
    path_(path),
    dirty_(false),
    lastUpScoreTime_(0),
    cacheLastSave_(0)
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
    ABC_Debug(2, "ServerCache::load()");

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

        // If there is a cached server that is not on the auth server list, then set it's score to -1
        // to reduce chances of using it.
        bool serversMatch = false;
        size_t size = bitcoinServers.size();
        for (size_t i = 0; i < size; i++)
        {
            std::string serverUrlNew = bitcoinServers[i];
            if (boost::equal(serverUrl, serverUrlNew))
            {
                serversMatch = true;
                break;
            }
        }
        if (!serversMatch)
        {
            ABC_Debug(1, "ServerCache::load Missing from auth " + serverUrl);
            if (serverScore >= 0)
            {
                serverScore = -1;
            }
        }

        unsigned long serverResponseTime = ssj.serverResponseTime();
        ServerInfo serverInfo;
        serverInfo.serverUrl = serverUrl;
        // Level the playing field a little bit on each bootup by maxing out all scores by 100 less than MAX_SCORE
        if (0 == cacheLastSave_)
            serverInfo.score = serverScore > MAX_SCORE - 100 ? MAX_SCORE - 100 :
                               serverScore;
        else
            serverInfo.score = serverScore;
        serverInfo.responseTime = serverResponseTime;
        serverInfo.numResponseTimes = 0;
        servers_[serverUrl] = serverInfo;
        ABC_DebugLevel(1, "ServerCache::load %d %d ms %s",
                       serverInfo.score, serverInfo.responseTime, serverInfo.serverUrl.c_str())

    }

    return save_nolock();
}

Status
ServerCache::save_nolock()
{
    ABC_Debug(2, "ServerCache::save()");
    JsonArray serverScoresJsonArray;

    if (dirty_)
    {
        time_t now = time(nullptr);

        if (10 <= now - cacheLastSave_)
        {
            cacheLastSave_ = now;
            std::vector<ServerInfo> serverInfos;

            for (const auto &server: servers_)
            {
                // Copy from map to vector so we can sort it.
                serverInfos.push_back(server.second);
            }
            std::sort(serverInfos.begin(), serverInfos.end(), sortServersByScore);
            for (const auto &serverInfo: serverInfos)
            {
                ServerScoreJson ssj;
                ABC_CHECK(ssj.serverUrlSet(serverInfo.serverUrl));
                ABC_CHECK(ssj.serverScoreSet(serverInfo.score));
                ABC_CHECK(ssj.serverResponseTimeSet(serverInfo.responseTime));
                ABC_CHECK(serverScoresJsonArray.append(ssj));
                ABC_DebugLevel(2, "ServerCache::save %d %d ms %s",
                               serverInfo.score, serverInfo.responseTime, serverInfo.serverUrl.c_str())
            }
            ABC_CHECK(serverScoresJsonArray.save(path_));
            dirty_ = false;
        }
        else
        {
            ABC_Debug(1, "ServerCache::save() NOT SAVED. TOO SOON");
        }
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
        ServerInfo serverInfo = svr->second;
        serverInfo.score += changeScore;
        if (serverInfo.score > MAX_SCORE)
            serverInfo.score = MAX_SCORE;
        servers_[serverUrl] = serverInfo;
        dirty_ = true;
        ABC_Debug(1, "serverScoreUp:" + serverUrl + " " + std::to_string(
                      serverInfo.score));
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
        ServerInfo serverInfo = svr->second;
        serverInfo.score -= changeScore;
        if (serverInfo.score < MIN_SCORE)
            serverInfo.score = MIN_SCORE;
        servers_[serverUrl] = serverInfo;
        dirty_ = true;
        ABC_Debug(2, "serverScoreDown:" + serverUrl + " " + std::to_string(
                      serverInfo.score));
    }
    return Status();
}

unsigned long long
ServerCache::getCurrentTimeMilliSeconds()
{
    struct timeval tv;

    gettimeofday(&tv, NULL);

    unsigned long long millisecondsSinceEpoch =
        (unsigned long long)(tv.tv_sec) * 1000 +
        (unsigned long long)(tv.tv_usec) / 1000;

    return millisecondsSinceEpoch;
}

void
ServerCache::setResponseTime(std::string serverUrl,
                             unsigned long long responseTimeMilliseconds)
{
    // Collects that last 10 response time values to provide an average response time.
    // This is used in weighting the score of a particular server
    auto svr = servers_.find(serverUrl);
    if (servers_.end() != svr)
    {
        ServerInfo serverInfo = svr->second;
        serverInfo.numResponseTimes++;

        unsigned long long oldtime = serverInfo.responseTime;
        unsigned long long newTime = 0;
        if (RESPONSE_TIME_UNINITIALIZED == oldtime)
        {
            newTime = responseTimeMilliseconds;
        }
        else
        {
            // Every 10th setting of response time, decrease effect of prior values by 5x
            if (serverInfo.numResponseTimes % 10 == 0)
            {
                newTime = (oldtime + (responseTimeMilliseconds * 4)) / 5;
            }
            else
            {
                newTime = (oldtime + responseTimeMilliseconds) / 2;
            }
        }
        serverInfo.responseTime = newTime;
        servers_[serverUrl] = serverInfo;
        ABC_Debug(2, "setResponseTime:" + serverUrl + " oldTime:" + std::to_string(
                      oldtime) + " newTime:" + std::to_string(newTime));
    }
}

std::vector<std::string>
ServerCache::getServers(ServerType type, unsigned int numServersWanted)
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<ServerInfo> serverInfos;
    std::vector<ServerInfo> newServerInfos;
    std::vector<std::string> servers;

    servers.clear();

    // Get all the servers that match the type

    if (servers_.empty())
        return servers;

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

        serverInfos.push_back(server.second);
        ServerInfo serverInfo = server.second;

        // If this is a new server, save it for use later
        if (serverInfo.responseTime == RESPONSE_TIME_UNINITIALIZED &&
                serverInfo.score == 0)
            newServerInfos.push_back(serverInfo);

        ABC_DebugLevel(2, "getServers unsorted: %d %d ms %s",
                       serverInfo.score, serverInfo.responseTime, serverInfo.serverUrl.c_str())

    }

    // Sort by score
    std::sort(serverInfos.begin(), serverInfos.end(), sortServersByScore);

    //
    // Take the top 50% of servers that have
    // 1. A score between 100 points of the highest score
    // 2. A positive score of at least 5
    // 3. A response time that is not RESPONSE_TIME_UNINITIALIZED
    //
    // Then sort those top servers by response time from lowest to highest
    //
    auto serverStart = serverInfos.begin();
    auto serverEnd = serverStart;
    int size = serverInfos.size();
    ServerInfo startServerInfo = *serverStart;
    int numServersPass = 0;
    for (auto it = serverInfos.begin(); it != serverInfos.end(); ++it)
    {
        ServerInfo serverInfo = *it;
        ABC_DebugLevel(2, "getServers sorted 1: %d %d ms %s",
                       serverInfo.score, serverInfo.responseTime, serverInfo.serverUrl.c_str())
        if (serverInfo.score < startServerInfo.score - 100)
            continue;
        if (serverInfo.score <= 5)
            continue;
        if (serverInfo.responseTime >= RESPONSE_TIME_UNINITIALIZED)
            continue;

        numServersPass++;
        if (numServersPass >= numServersWanted)
            continue;
        if (numServersPass >= size / 2)
            continue;
        serverEnd = it;
    }

    std::sort(serverStart, serverEnd, sortServersByTime);


    int numNewServers = 0;
    int numServers = 0;
    for (auto it = serverInfos.begin(); it != serverInfos.end(); ++it)
    {
        ServerInfo serverInfo = *it;
        ABC_DebugLevel(2, "getServers sorted 2: %d %d ms %s",
                       serverInfo.score, serverInfo.responseTime, serverInfo.serverUrl.c_str())
        servers.push_back(serverInfo.serverUrl);
        numServers++;
        if (serverInfo.responseTime == RESPONSE_TIME_UNINITIALIZED &&
                serverInfo.score == 0)
            numNewServers++;


        if (numServersWanted <= numServers)
            break;

        // Try to fill half of the number of requested servers with new, untried servers so that
        // we eventually try the full list of servers to score them.
        int halfServersWanted = roundUpDivide(numServersWanted, 2);
        if (numServers >= halfServersWanted &&
                numNewServers == 0)
        {
            if (newServerInfos.size() >= (numServersWanted - numServers))
                break;
        }
    }

    // If this list does not have a new server in it, try to add one as we always want to give new
    // servers a try.
    if (0 == numNewServers)
    {
        for (auto it = newServerInfos.begin(); it != newServerInfos.end(); ++it)
        {
            ServerInfo serverInfo = *it;
            auto it2 = servers.begin();
            servers.insert(it2, serverInfo.serverUrl);
            ABC_DebugLevel(2, "getServers sorted 2+: %d %d ms %s",
                           serverInfo.score, serverInfo.responseTime, serverInfo.serverUrl.c_str())

            numServers++;
            if (numServers >= numServersWanted)
                break;
        }
    }
    return servers;

}
} // namespace abcd
