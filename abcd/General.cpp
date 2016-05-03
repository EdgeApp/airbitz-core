/*
 * Copyright (c) 2014, Airbitz, Inc.
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

constexpr unsigned fallbackFee = 10000; // Satoshi per KB

#define FALLBACK_BITCOIN_SERVERS {  "tcp://obelisk.airbitz.co:9091", \
                                    "stratum://stratum-az-wusa.airbitz.co:50001", \
                                    "stratum://stratum-az-wjapan.airbitz.co:50001", \
                                    "stratum://stratum-az-neuro.airbitz.co:50001" }
#define TESTNET_BITCOIN_SERVERS {   "tcp://obelisk-testnet.airbitz.co:9091" }
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
    ABC_JSON_NUMBER(incomingRate, "incomingRate", 0)
    ABC_JSON_INTEGER(incomingMax, "incomingMax", 0)
    ABC_JSON_INTEGER(incomingMin, "incomingMin", 0)
    ABC_JSON_NUMBER(outgoingPercentage, "percentage", 0)
    ABC_JSON_INTEGER(outgoingMax, "maxSatoshi", 0)
    ABC_JSON_INTEGER(outgoingMin, "minSatoshi", 0)
    ABC_JSON_INTEGER(noFeeMinSatoshi, "noFeeMinSatoshi", 0)
    ABC_JSON_INTEGER(sendMin, "sendMin", 4000) // No dust allowed
    ABC_JSON_INTEGER(sendPeriod, "sendPeriod", 7*24*60*60) // One week
    ABC_JSON_STRING(sendPayee, "sendPayee", "Airbitz")
    ABC_JSON_STRING(sendCategory, "sendCategory", "Expense:Fees")
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

    const auto path = gContext->paths.generalPath();
    if (!fileExists(path))
        generalUpdate().log();

    GeneralJson out;
    out.load(path).log();
    return out;
}

Status
generalUpdate()
{
    const auto path = gContext->paths.generalPath();

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
        out[1000] = fallbackFee;
    return out;
}

AirbitzFeeInfo
generalAirbitzFeeInfo()
{
    AirbitzFeeInfo out;
    auto feeJson = generalLoad().airbitzFees();

    auto arrayJson = feeJson.addresses();
    size_t size = arrayJson.size();
    for (size_t i = 0; i < size; i++)
    {
        auto stringJson = arrayJson[i];
        if (json_is_string(stringJson.get()))
            out.addresses.insert(json_string_value(stringJson.get()));
    }

    out.incomingRate = feeJson.incomingRate();
    out.incomingMin  = feeJson.incomingMin();
    out.incomingMax  = feeJson.incomingMax();

    out.outgoingRate = feeJson.outgoingPercentage() / 100.0;
    out.outgoingMin  = feeJson.outgoingMin();
    out.outgoingMax  = feeJson.outgoingMax();
    out.noFeeMinSatoshi = feeJson.noFeeMinSatoshi();

    out.sendMin = feeJson.sendMin();
    out.sendPeriod = feeJson.sendPeriod();
    out.sendPayee = feeJson.sendPayee();
    out.sendCategory = feeJson.sendCategory();

    return out;
}

uint64_t
generalAirbitzFee(const AirbitzFeeInfo &info, uint64_t spend, bool transfer)
{
    int64_t fee = info.outgoingRate * spend;

    if (info.addresses.empty())
        return 0;
    if (transfer)
        return 0;
    if (fee < info.noFeeMinSatoshi)
        return 0;
    if (fee < info.outgoingMin)
        return info.outgoingMin;
    if (info.outgoingMax < fee)
        return info.outgoingMax;
    return fee;
}

std::vector<std::string>
generalBitcoinServers()
{
    std::vector<std::string> out;

    if (isTestnet())
    {
        std::string serverlist[] = TESTNET_BITCOIN_SERVERS;

        size_t size = sizeof(serverlist) / sizeof(*serverlist);
        for (size_t i = 0; i < size; i++)
            out.push_back(serverlist[i]);

        return out;
    }

    auto arrayJson = generalLoad().bitcoinServers();

    size_t size = arrayJson.size();
    out.reserve(size);
    for (size_t i = 0; i < size; i++)
    {
        auto stringJson = arrayJson[i];
        if (json_is_string(stringJson.get()))
        {
            std::string servername = json_string_value(stringJson.get());
            out.push_back(servername);
        }
    }

    if (!out.size())
    {
        std::string serverlist[] = FALLBACK_BITCOIN_SERVERS;

        size_t size = sizeof(serverlist) / sizeof(*serverlist);
        for (size_t i = 0; i < size; i++)
            out.push_back(serverlist[i]);
    }
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
