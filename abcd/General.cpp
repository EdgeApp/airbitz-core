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
#include "bitcoin/Testnet.hpp"
#include "json/JsonObject.hpp"
#include "json/JsonArray.hpp"
#include "login/server/LoginServer.hpp"
#include "util/FileIO.hpp"
#include "util/Debug.hpp"
#include "http/HttpRequest.hpp"
#include <time.h>
#include <mutex>

namespace abcd {

#define FALLBACK_BITCOIN_SERVERS {  "tcp://obelisk.airbitz.co:9091", \
                                    "stratum://stratum-az-wusa.airbitz.co:50001", \
                                    "stratum://stratum-az-wjapan.airbitz.co:50001", \
                                    "stratum://stratum-az-neuro.airbitz.co:50001" }
#define TESTNET_BITCOIN_SERVERS {   "tcp://obelisk-testnet.airbitz.co:9091", \
                                    "stratum://electrum-bctest.airbitz.co:50001" }
#define GENERAL_ACCEPTABLE_INFO_FILE_AGE_SECS   (8 * 60 * 60) // how many seconds old can the info file be before it should be updated
#define ESTIMATED_FEES_ACCEPTABLE_INFO_FILE_AGE_SECS   (3 * 60 * 60) // how many seconds old can the info file be before it should be updated

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

struct BitcoinFeesJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(BitcoinFeesJson, JsonObject)
    ABC_JSON_INTEGER(confirmFees1, "confirmFees1", 220001)
    ABC_JSON_INTEGER(confirmFees2, "confirmFees2", 200001)
    ABC_JSON_INTEGER(confirmFees3, "confirmFees3", 180001)
    ABC_JSON_INTEGER(confirmFees4, "confirmFees4", 160001)
    ABC_JSON_INTEGER(confirmFees5, "confirmFees5", 140001)
    ABC_JSON_INTEGER(confirmFees6, "confirmFees6", 120001)
    ABC_JSON_INTEGER(confirmFees7, "confirmFees7", 100001)
    ABC_JSON_INTEGER(highFeeBlock, "highFeeBlock", 1)
    ABC_JSON_INTEGER(standardFeeBlockHigh, "standardFeeBlockHigh", 2)
    ABC_JSON_INTEGER(standardFeeBlockLow, "standardFeeBlockLow", 5)
    ABC_JSON_INTEGER(lowFeeBlock, "lowFeeBlock", 7)
    ABC_JSON_NUMBER(targetFeePercentage, "targetFeePercentage", 1.0)
};

struct EstimateFeesJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(EstimateFeesJson, JsonObject)
    ABC_JSON_INTEGER(confirmFees1, "confirmFees1", 0)
    ABC_JSON_INTEGER(confirmFees2, "confirmFees2", 0)
    ABC_JSON_INTEGER(confirmFees3, "confirmFees3", 0)
    ABC_JSON_INTEGER(confirmFees4, "confirmFees4", 0)
    ABC_JSON_INTEGER(confirmFees5, "confirmFees5", 0)
    ABC_JSON_INTEGER(confirmFees6, "confirmFees6", 0)
    ABC_JSON_INTEGER(confirmFees7, "confirmFees7", 0)
};

struct TwentyOneFeesJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(TwentyOneFeesJson, JsonObject)
    ABC_JSON_VALUE(fees, "fees", JsonArray)
};

struct TwentyOneFeeJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(TwentyOneFeeJson, JsonObject)
    ABC_JSON_INTEGER(minFee, "minFee", 0)
    ABC_JSON_INTEGER(maxFee, "maxFee", 0)
    ABC_JSON_INTEGER(dayCount, "dayCount", 0)
    ABC_JSON_INTEGER(memCount, "memCount", 0)
    ABC_JSON_INTEGER(minDelay, "minDelay", 0)
    ABC_JSON_INTEGER(maxDelay, "maxDelay", 0)
    ABC_JSON_INTEGER(minMinutes, "minMinutes", 0)
    ABC_JSON_INTEGER(maxMinutes, "maxMinutes", 0)
};

struct GeneralJson:
    public JsonObject
{
    ABC_JSON_VALUE(bitcoinFees,    "minersFees2", BitcoinFeesJson)
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
general21FeesUpdate()
{
    HttpReply reply;
    const auto url = "https://bitcoinfees.21.co/api/v1/fees/list";
    const auto path = gContext->paths.twentyOneFeeCachePath();

    ABC_CHECK(HttpRequest()
              .get(reply, url));
    ABC_CHECK(reply.codeOk());

    TwentyOneFeesJson feesJson;
    ABC_CHECK(feesJson.decode(reply.body));
    ABC_CHECK(feesJson.save(path));

    return Status();
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
    general21FeesUpdate();

    return Status();
}

static EstimateFeesJson
estimateFeesLoad()
{
    if (!gContext)
        return EstimateFeesJson();

    const auto path = gContext->paths.feeCachePath();
    if (!fileExists(path))
        return EstimateFeesJson();

    EstimateFeesJson out;
    out.load(path).log();
    return out;
}

static TwentyOneFeesJson
twentyOneFeesLoad()
{
    if (!gContext)
        return TwentyOneFeesJson();

    const auto path = gContext->paths.twentyOneFeeCachePath();
    if (!fileExists(path))
        return TwentyOneFeesJson();

    TwentyOneFeesJson out;
    out.load(path).log();
    return out;
}

bool
generalEstimateFeesNeedUpdate()
{
    const auto path = gContext->paths.feeCachePath();

    time_t lastTime;
    if (!fileTime(lastTime, path) ||
            lastTime + ESTIMATED_FEES_ACCEPTABLE_INFO_FILE_AGE_SECS < time(nullptr))
    {
        return true;
    }

    return false;
}

static bool estimatedFeesInitialized = 0;
static double estimatedFees[MAX_FEES_BLOCKS];
static size_t estimatedFeesNumResponses[MAX_FEES_BLOCKS];
static std::mutex mutex_;

Status
generalEstimateFeesUpdate(size_t blocks, double fee)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!estimatedFeesInitialized)
    {
        estimatedFees[1] = estimatedFeesNumResponses[1] = 0;
        estimatedFees[2] = estimatedFeesNumResponses[2] = 0;
        estimatedFees[3] = estimatedFeesNumResponses[3] = 0;
        estimatedFees[4] = estimatedFeesNumResponses[4] = 0;
        estimatedFees[5] = estimatedFeesNumResponses[5] = 0;
        estimatedFees[6] = estimatedFeesNumResponses[6] = 0;
        estimatedFees[7] = estimatedFeesNumResponses[7] = 0;
        estimatedFeesInitialized = true;
    }

    // If the passed in fee is negative (commonly -1) then use the fee
    // of one larger block delay
    if (fee < 0)
    {
        if (estimatedFeesNumResponses[blocks] == 0)
        {
            if (blocks < 7)
            {
                if (estimatedFees[blocks + 1] > 0)
                {
                    fee = estimatedFees[blocks + 1] / 100000000.0;
                    if (blocks == 1)
                        fee *= 1.2;
                }
            }
        }
    }

    if (fee < 0)
        return Status();

    // Take the average of all the responses for a given block target
    uint64_t tempFee = (estimatedFees[blocks] * estimatedFeesNumResponses[blocks])
                       + (uint64_t) (fee * 100000000.0);
    estimatedFeesNumResponses[blocks]++;
    estimatedFees[blocks] = tempFee / estimatedFeesNumResponses[blocks];

    if (estimatedFees[1] > 0 &&
            estimatedFees[2] > 0 &&
            estimatedFees[3] > 0 &&
            estimatedFees[4] > 0 &&
            estimatedFees[5] > 0 &&
            estimatedFees[6] > 0 &&
            estimatedFees[7] > 0)
    {
        // Save the fees in a Json file
        EstimateFeesJson feesJson;
        feesJson.confirmFees1Set(estimatedFees[1]);
        feesJson.confirmFees2Set(estimatedFees[2]);
        feesJson.confirmFees3Set(estimatedFees[3]);
        feesJson.confirmFees4Set(estimatedFees[4]);
        feesJson.confirmFees5Set(estimatedFees[5]);
        feesJson.confirmFees6Set(estimatedFees[6]);
        feesJson.confirmFees7Set(estimatedFees[7]);

        const auto path = gContext->paths.feeCachePath();

        ABC_CHECK(feesJson.save(path));
    }
    return Status();
}

const double MAX_FEE = 999999999.0;
const int MAX_STANDARD_DELAY = 12;
const int MIN_STANDARD_DELAY = 3;

BitcoinFeeInfo
generalBitcoinFeeInfo()
{
    BitcoinFeesJson feeJson = generalLoad().bitcoinFees();
    EstimateFeesJson estimateFeesJson = estimateFeesLoad();
    TwentyOneFeesJson twentyOneFeesJson = twentyOneFeesLoad();

    auto arrayJson = twentyOneFeesJson.fees();
    size_t size = arrayJson.size();

    int highDelay = 999999;
    int lowDelay = 0;
    double highFee = MAX_FEE;
    double standardFeeHigh = 0;
    double standardFeeLow = MAX_FEE;
    double lowFee = MAX_FEE;

    //
    // Grab 21.co fee info and see if we have a complete set from the last update
    //
    for (size_t i = 0; i < size; i++)
    {
        // Iterate over all the fee estimates
        TwentyOneFeeJson twentyOneFeeJson(arrayJson[i]);

        ABC_DebugLevel(1,
                "minFee:%d,maxFee:%d,minDelay:%d,maxDelay:%d,minMinutes:%d,maxMinutes:%d",
                twentyOneFeeJson.minFee(), twentyOneFeeJson.maxFee(),
                twentyOneFeeJson.minDelay(),
                twentyOneFeeJson.maxDelay(), twentyOneFeeJson.minMinutes(),
                twentyOneFeeJson.maxMinutes());
                
        // If this is a zero fee estimate, then skip
        if (twentyOneFeeJson.maxFee() == 0 ||
            twentyOneFeeJson.minFee() == 0) 
        {
            continue;
        }
        
        // Set the lowFee if the delay in blocks and minutes is less that 10000.
        // 21.co uses 10000 to mean infinite
        if (twentyOneFeeJson.maxDelay() < 10000
                && twentyOneFeeJson.maxMinutes() < 10000)
            if (twentyOneFeeJson.maxFee() < lowFee)
            {
                // Set the low fee if the current fee estimate is lower than the previously set low fee
                lowDelay = twentyOneFeeJson.maxDelay();
                lowFee = (double) twentyOneFeeJson.maxFee();
            }

        // Set the high fee only if the delay is 0
        if (twentyOneFeeJson.maxDelay() == 0)
            if (twentyOneFeeJson.maxFee() < highFee)
            {
                // Set the low fee if the current fee estimate is lower than the previously set high fee
                highFee = (double) twentyOneFeeJson.maxFee();
                highDelay = twentyOneFeeJson.maxDelay();
            }
    }

    // Now find the standard fee range. We want the range to be within a maxDelay of
    // 3 to 18 blocks (about 30 mins to 3 hours). The standard fee at the low end should
    // have a delay less than the lowFee from above. The standard fee at the high end
    // should have a delay that's greater than the highFee from above.
    for (size_t i = 0; i < size; i++)
    {
        TwentyOneFeeJson twentyOneFeeJson(arrayJson[i]);

        // If this is a zero fee estimate, then skip
        if (twentyOneFeeJson.maxFee() == 0 ||
            twentyOneFeeJson.minFee() == 0) 
        {
            continue;
        }

        if (twentyOneFeeJson.maxDelay() < lowDelay &&
                twentyOneFeeJson.maxDelay() <= MAX_STANDARD_DELAY)
            if (standardFeeLow > twentyOneFeeJson.minFee())
                standardFeeLow = (double) twentyOneFeeJson.minFee();
    }

    // Go backwards looking for lowest standardFeeHigh that:
    // 1. Is < highFee
    // 2. Has a blockdelay > highDelay
    // 3. Has a delay that is > MIN_STANDARD_DELAY
    // Use the highFee as the default standardHighFee 
    standardFeeHigh = highFee;
    for (size_t i = size - 1; i >= 0; i--)
    {
        TwentyOneFeeJson twentyOneFeeJson(arrayJson[i]);

        // If this is a zero fee estimate, then skip
        if (twentyOneFeeJson.maxFee() == 0 ||
            twentyOneFeeJson.minFee() == 0) 
        {
            continue;
        }

        // Dont ever go below standardFeeLow
        if (twentyOneFeeJson.maxFee() <= standardFeeLow)
            break;
        
        if (twentyOneFeeJson.maxDelay() > highDelay)
            standardFeeHigh = (double) twentyOneFeeJson.maxFee();

        // If we have a delay that's greater than MIN_STANDARD_DELAY, then we're done.
        // Otherwise we'd be getting bigger delays and further reducing fees. 
        if (twentyOneFeeJson.maxDelay() > MIN_STANDARD_DELAY)
            break;
    }

    BitcoinFeeInfo out;

    out.targetFeePercentage = feeJson.targetFeePercentage();

    //
    // Check if we have a complete set of fee info.
    //
    if (highFee < MAX_FEE &&
        lowFee  < MAX_FEE &&
        standardFeeHigh > 0 &&
        standardFeeLow < MAX_FEE)
    {
        // Complete set found. Assign the confirmFees array based on the 21.co fees
        out.confirmFees[1] = highFee * 1000;
        out.confirmFees[2] = standardFeeHigh * 1000;
        out.confirmFees[3] = standardFeeLow * 1000;
        out.confirmFees[4] = lowFee * 1000;
        out.confirmFees[5] = lowFee * 1000;
        out.confirmFees[6] = lowFee * 1000;
        out.confirmFees[7] = lowFee * 1000;
        out.highFeeBlock            = 1;
        out.standardFeeBlockHigh    = 2;
        out.standardFeeBlockLow     = 3;
        out.lowFeeBlock             = 4;
    }
    else
    {
        // Complete set not found. Use the bitcoind/stratum fee estimates
        out.confirmFees[1] = estimateFeesJson.confirmFees1() ?
                             estimateFeesJson.confirmFees1() : feeJson.confirmFees1();
        out.confirmFees[2] = estimateFeesJson.confirmFees2() ?
                             estimateFeesJson.confirmFees2() : feeJson.confirmFees2();
        out.confirmFees[3] = estimateFeesJson.confirmFees3() ?
                             estimateFeesJson.confirmFees3() : feeJson.confirmFees3();
        out.confirmFees[4] = estimateFeesJson.confirmFees4() ?
                             estimateFeesJson.confirmFees4() : feeJson.confirmFees4();
        out.confirmFees[5] = estimateFeesJson.confirmFees5() ?
                             estimateFeesJson.confirmFees5() : feeJson.confirmFees5();
        out.confirmFees[6] = estimateFeesJson.confirmFees6() ?
                             estimateFeesJson.confirmFees6() : feeJson.confirmFees6();
        out.confirmFees[7] = estimateFeesJson.confirmFees7() ?
                             estimateFeesJson.confirmFees7() : feeJson.confirmFees7();
        out.lowFeeBlock             = feeJson.lowFeeBlock();
        out.standardFeeBlockLow     = feeJson.standardFeeBlockLow();
        out.standardFeeBlockHigh    = feeJson.standardFeeBlockHigh();
        out.highFeeBlock            = feeJson.highFeeBlock();
    }

    // Fix any fees that contradict. ie. confirmFees1 < confirmFees2
    if (out.confirmFees[2] > out.confirmFees[1])
        out.confirmFees[2] = out.confirmFees[1];
    if (out.confirmFees[3] > out.confirmFees[2])
        out.confirmFees[3] = out.confirmFees[2];
    if (out.confirmFees[4] > out.confirmFees[3])
        out.confirmFees[4] = out.confirmFees[3];
    if (out.confirmFees[5] > out.confirmFees[4])
        out.confirmFees[5] = out.confirmFees[4];
    if (out.confirmFees[6] > out.confirmFees[5])
        out.confirmFees[6] = out.confirmFees[5];
    if (out.confirmFees[7] > out.confirmFees[6])
        out.confirmFees[7] = out.confirmFees[6];

    for (int i = 1; i <= 7; i++) {
        if (out.confirmFees[i] == 0) {
            out.confirmFees[i] = 1000;
        }
    }

    ABC_DebugLevel(1,
                   "generalBitcoinFeeInfo: 1:%.0f, 2:%.0f, 3:%.0f, 4:%.0f, 5:%.0f, 6:%.0f, 7:%.0f",
                   out.confirmFees[1], out.confirmFees[2], out.confirmFees[3], out.confirmFees[4],
                   out.confirmFees[5], out.confirmFees[6], out.confirmFees[7]);

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
    {
        out.push_back("https://git.airbitz.co/repos");
        out.push_back("https://git1.airbitz.co/repos");
        out.push_back("https://git2.airbitz.co/repos");
        out.push_back("https://git4.airbitz.co/repos");
    }

    std::random_shuffle(out.begin(), out.end());

    return out;
}

} // namespace abcd
