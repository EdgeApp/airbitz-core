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

namespace abcd {

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
    dirty_(false)
{
}

void
ServerCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    servers_.clear();
}

static JsonArray
serverScoresLoad(std::string path)
{
    JsonArray out;

    if (!gContext)
        return out;

    if (!fileExists(path))
        return out;

    out.load(path).log();
    return out;
}

Status
ServerCache::load()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Add any new servers
    JsonArray serverScoresJson = serverScoresLoad(path_);

    std::vector<std::string> bitcoinServers = generalBitcoinServers();

    size_t size = bitcoinServers.size();
    for (size_t i = 0; i < size; i++)
    {
        std::string serverUrlNew = bitcoinServers[i];

        size_t sizeScores = serverScoresJson.size();
        bool serversMatch = false;
        for (size_t j = 0; j < sizeScores; j++)
        {
            ServerScoreJson ssj = serverScoresJson[j];
            std::string serverUrl(ssj.serverUrl());

            if (boost::iequals(serverUrl, serverUrlNew))
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
            serverScoresJson.append(ssjNew);
        }
    }
    serverScoresJson.save(scoresPath);

    return Status();
}

Status
ServerCache::save()
{
    std::lock_guard<std::mutex> lock(mutex_);




    if (dirty_)
    {
        for (const auto &server: servers_)
        {

        }
    }
//        BlockCacheJson json;
//        ABC_CHECK(json.heightSet(height_));
//
//        JsonArray headersJson;
//        for (const auto &header: headers_)
//        {
//            bc::data_chunk rawHeader(satoshi_raw_size(header.second));
//            bc::satoshi_save(header.second, rawHeader.begin());
//
//            BlockHeaderJson blockHeaderJson;
//            ABC_CHECK(blockHeaderJson.heightSet(header.first));
//            ABC_CHECK(blockHeaderJson.headerSet(base64Encode(rawHeader)));
//            ABC_CHECK(headersJson.append(blockHeaderJson));
//        }
//        ABC_CHECK(json.headersSet(headersJson));
//
//        ABC_CHECK(json.save(path_));
//        dirty_ = false;
//    }

    return Status();
}


} // namespace abcd
