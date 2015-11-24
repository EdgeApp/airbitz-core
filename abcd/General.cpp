/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * AirBitz general, non-account-specific server-supplied data.
 *
 * The data handled in this file is basically just a local cache of various
 * settings that AirBitz would like to adjust from time-to-time without
 * upgrading the entire app.
 */

#include "General.hpp"
#include "Context.hpp"
#include "auth/LoginServer.hpp"
#include "bitcoin/Testnet.hpp"
#include "json/JsonObject.hpp"
#include "json/JsonArray.hpp"
#include "util/FileIO.hpp"
#include <time.h>

namespace abcd {

constexpr unsigned fallbackFee = 10000;

#define GENERAL_INFO_FILENAME                   "Servers.json"
#define GENERAL_ACCEPTABLE_INFO_FILE_AGE_SECS   (24 * 60 * 60) // how many seconds old can the info file before it should be updated

struct BitcoinFeeJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(BitcoinFeeJson, JsonObject)
    ABC_JSON_INTEGER(size, "txSizeBytes", 0)
    ABC_JSON_INTEGER(fee, "feeSatoshi", fallbackFee)
};

struct AirbitzFeesJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(AirbitzFeesJson, JsonObject)
    ABC_JSON_VALUE(addresses, "addresses", JsonArray)
    ABC_JSON_INTEGER(maxSatoshi, "maxSatoshi", 0)
    ABC_JSON_INTEGER(minSatoshi, "minSatoshi", 0)
    ABC_JSON_NUMBER(percentage, "percentage", 0)
};

struct GeneralJson:
    public JsonObject
{
    ABC_JSON_VALUE(bitcoinFees,    "minersFees", JsonArray)
    ABC_JSON_VALUE(airbitzFees,    "feesAirBitz", AirbitzFeesJson)
    ABC_JSON_VALUE(bitcoinServers, "obeliskServers", JsonArray)
    ABC_JSON_VALUE(syncServers,    "syncServers", JsonArray)
};

/**
 * Attempts to load the general information from disk.
 */
static GeneralJson
generalLoad()
{
    if (!gContext)
        return GeneralJson();

    const auto path = gContext->rootDir() + GENERAL_INFO_FILENAME;
    if (!fileExists(path))
        generalUpdate().log();

    GeneralJson out;
    out.load(path).log();
    return out;
}

Status
generalUpdate()
{
    const auto path = gContext->rootDir() + GENERAL_INFO_FILENAME;

    time_t lastTime;
    if (!fileTime(lastTime, path) ||
        lastTime + GENERAL_ACCEPTABLE_INFO_FILE_AGE_SECS < time(nullptr))
    {
        JsonPtr infoJson;
        ABC_CHECK(loginServerGetGeneral(infoJson));
        ABC_CHECK(infoJson.save(path));
    }

    return Status();
}

BitcoinFeeInfo
generalBitcoinFeeInfo()
{
    auto arrayJson = generalLoad().bitcoinFees();

    BitcoinFeeInfo out;
    size_t size = arrayJson.size();
    for (size_t i = 0; i < size; i++)
    {
        BitcoinFeeJson feeJson = arrayJson[i];
        out[feeJson.size()] = feeJson.fee();
    }

    if (!out.size())
        out[0] = fallbackFee;
    return out;
}

AirbitzFeeInfo
generalAirbitzFeeInfo()
{
    auto feeJson = generalLoad().airbitzFees();

    AirbitzFeeInfo out;
    out.minSatoshi = feeJson.minSatoshi();
    out.maxSatoshi = feeJson.maxSatoshi();
    out.rate = feeJson.percentage() / 100.0;

    auto arrayJson = feeJson.addresses();
    size_t size = arrayJson.size();
    for (size_t i = 0; i < size; i++)
    {
        auto stringJson = arrayJson[i];
        if (json_is_string(stringJson.get()))
            out.addresses.insert(json_string_value(stringJson.get()));
    }

    if (!out.addresses.size())
        return AirbitzFeeInfo{{}, 0, 0, 0};
    return out;
}

std::vector<std::string>
generalBitcoinServers()
{
    if (isTestnet())
    {
        return std::vector<std::string>{
            "tcp://obelisk-testnet.airbitz.co:9091"
        };
    }

    auto arrayJson = generalLoad().bitcoinServers();

    std::vector<std::string> out;
    size_t size = arrayJson.size();
    out.reserve(size);
    for (size_t i = 0; i < size; i++)
    {
        auto stringJson = arrayJson[i];
        if (json_is_string(stringJson.get()))
            out.push_back(json_string_value(stringJson.get()));
    }

    if (!out.size())
        out.push_back("tcp://obelisk.airbitz.co:9091");
    return out;
}

std::vector<std::string>
generalSyncServers()
{
    auto arrayJson = generalLoad().syncServers();

    std::vector<std::string> out;
    size_t size = arrayJson.size();
    out.reserve(size);
    for (size_t i = 0; i < size; i++)
    {
        auto stringJson = arrayJson[i];
        if (json_is_string(stringJson.get()))
            out.push_back(json_string_value(stringJson.get()));
    }

    if (!out.size())
        out.push_back("https://git.sync.airbitz.co/repos");
    return out;
}

} // namespace abcd
