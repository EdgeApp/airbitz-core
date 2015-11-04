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
#include "util/Debug.hpp"
#include "util/FileIO.hpp"
#include "util/Json.hpp"
#include "util/Util.hpp"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <jansson.h>

namespace abcd {

#define FALLBACK_OBELISK "tcp://obelisk.airbitz.co:9091"
#define TESTNET_OBELISK "tcp://obelisk-testnet.airbitz.co:9091"

#define GENERAL_INFO_FILENAME                   "Servers.json"
#define GENERAL_ACCEPTABLE_INFO_FILE_AGE_SECS   (24 * 60 * 60) // how many seconds old can the info file before it should be updated

#define JSON_INFO_MINERS_FEES_FIELD             "minersFees"
#define JSON_INFO_MINERS_FEE_SATOSHI_FIELD      "feeSatoshi"
#define JSON_INFO_MINERS_FEE_TX_SIZE_FIELD      "txSizeBytes"
#define JSON_INFO_AIRBITZ_FEES_FIELD            "feesAirBitz"
#define JSON_INFO_AIRBITZ_FEE_PERCENTAGE_FIELD  "percentage"
#define JSON_INFO_AIRBITZ_FEE_MAX_SATOSHI_FIELD "maxSatoshi"
#define JSON_INFO_AIRBITZ_FEE_MIN_SATOSHI_FIELD "minSatoshi"
#define JSON_INFO_AIRBITZ_FEE_ADDRESS_FIELD     "address"
#define JSON_INFO_OBELISK_SERVERS_FIELD         "obeliskServers"
#define JSON_INFO_SYNC_SERVERS_FIELD            "syncServers"

/**
 * Contains info on bitcoin miner fee
 */
typedef struct sABC_GeneralMinerFee
{
    uint64_t amountSatoshi;
    uint64_t sizeTransaction;
} tABC_GeneralMinerFee;

/**
 * Contains information on AirBitz fees
 */
typedef struct sABC_GeneralAirBitzFee
{
    double percentage; // maximum value 100.0
    uint64_t minSatoshi;
    uint64_t maxSatoshi;
    char *szAddresss;
} tABC_GeneralAirBitzFee;

/**
 * Contains general info from the server
 */
typedef struct sABC_GeneralInfo
{
    unsigned int            countMinersFees;
    tABC_GeneralMinerFee    **aMinersFees;
    tABC_GeneralAirBitzFee  *pAirBitzFee;
    unsigned int            countObeliskServers;
    char                    **aszObeliskServers;
    unsigned int            countSyncServers;
    char                    **aszSyncServers;
} tABC_GeneralInfo;

static void ABC_GeneralFreeInfo(tABC_GeneralInfo *pInfo);
static tABC_CC ABC_GeneralGetInfo(tABC_GeneralInfo **ppInfo, tABC_Error *pError);
static tABC_CC ABC_GeneralGetInfoFilename(char **pszFilename, tABC_Error *pError);

/**
 * Frees the general info struct.
 */
void ABC_GeneralFreeInfo(tABC_GeneralInfo *pInfo)
{
    if (pInfo)
    {
        if ((pInfo->aMinersFees != NULL) && (pInfo->countMinersFees > 0))
        {
            for (unsigned i = 0; i < pInfo->countMinersFees; i++)
            {
                ABC_CLEAR_FREE(pInfo->aMinersFees[i], sizeof(tABC_GeneralMinerFee));
            }
            ABC_CLEAR_FREE(pInfo->aMinersFees, sizeof(tABC_GeneralMinerFee *) * pInfo->countMinersFees);
        }

        if (pInfo->pAirBitzFee)
        {
            ABC_FREE_STR(pInfo->pAirBitzFee->szAddresss);
            ABC_CLEAR_FREE(pInfo->pAirBitzFee, sizeof(tABC_GeneralMinerFee));
        }

        if ((pInfo->aszObeliskServers != NULL) && (pInfo->countObeliskServers > 0))
        {
            for (unsigned i = 0; i < pInfo->countObeliskServers; i++)
            {
                ABC_FREE_STR(pInfo->aszObeliskServers[i]);
            }
            ABC_CLEAR_FREE(pInfo->aszObeliskServers, sizeof(char *) * pInfo->countObeliskServers);
        }

        if ((pInfo->aszSyncServers != NULL) && (pInfo->countSyncServers > 0))
        {
            for (unsigned i = 0; i < pInfo->countSyncServers; i++)
            {
                ABC_FREE_STR(pInfo->aszSyncServers[i]);
            }
            ABC_CLEAR_FREE(pInfo->aszSyncServers, sizeof(char *) * pInfo->countSyncServers);
        }

        ABC_CLEAR_FREE(pInfo, sizeof(tABC_GeneralInfo));
    }
}

/**
 * Load the general info.
 *
 * This function will load the general info which includes information on
 * Obelisk Servers, AirBitz fees and miners fees.
 */
tABC_CC ABC_GeneralGetInfo(tABC_GeneralInfo **ppInfo,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    JsonPtr file;
    json_t  *pJSON_Root             = NULL;
    json_t  *pJSON_Value            = NULL;
    char    *szInfoFilename         = NULL;
    json_t  *pJSON_MinersFeesArray  = NULL;
    json_t  *pJSON_AirBitzFees      = NULL;
    json_t  *pJSON_ObeliskArray     = NULL;
    json_t  *pJSON_SyncArray        = NULL;
    AutoFree<tABC_GeneralInfo, ABC_GeneralFreeInfo>
        pInfo(structAlloc<tABC_GeneralInfo>());

    ABC_CHECK_NULL(ppInfo);

    // get the info filename
    ABC_CHECK_RET(ABC_GeneralGetInfoFilename(&szInfoFilename, pError));

    // check to see if we have the file
    if (!fileExists(szInfoFilename))
    {
        // pull it down from the server
        ABC_CHECK_NEW(generalUpdate());
    }

    // load the json
    ABC_CHECK_NEW(file.load(szInfoFilename));
    pJSON_Root = file.get();

    // get the miners fees array
    pJSON_MinersFeesArray = json_object_get(pJSON_Root, JSON_INFO_MINERS_FEES_FIELD);
    ABC_CHECK_ASSERT((pJSON_MinersFeesArray && json_is_array(pJSON_MinersFeesArray)), ABC_CC_JSONError, "Error parsing JSON array value");

    // get the number of elements in the array
    pInfo->countMinersFees = (unsigned int) json_array_size(pJSON_MinersFeesArray);
    if (pInfo->countMinersFees > 0)
    {
        ABC_ARRAY_NEW(pInfo->aMinersFees, pInfo->countMinersFees, tABC_GeneralMinerFee*);
    }

    // run through all the miners fees
    for (unsigned i = 0; i < pInfo->countMinersFees; i++)
    {
        tABC_GeneralMinerFee *pFee = structAlloc<tABC_GeneralMinerFee>();

        // get the source object
        json_t *pJSON_Fee = json_array_get(pJSON_MinersFeesArray, i);
        ABC_CHECK_ASSERT((pJSON_Fee && json_is_object(pJSON_Fee)), ABC_CC_JSONError, "Error parsing JSON array element object");

        // get the satoshi amount
        pJSON_Value = json_object_get(pJSON_Fee, JSON_INFO_MINERS_FEE_SATOSHI_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pFee->amountSatoshi = (int) json_integer_value(pJSON_Value);

        // get the tranaction size
        pJSON_Value = json_object_get(pJSON_Fee, JSON_INFO_MINERS_FEE_TX_SIZE_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pFee->sizeTransaction = (int) json_integer_value(pJSON_Value);

        // assign this fee to the array
        pInfo->aMinersFees[i] = pFee;
    }

    // allocate the air bitz fees
    pInfo->pAirBitzFee = structAlloc<tABC_GeneralAirBitzFee>();

    // get the air bitz fees object
    pJSON_AirBitzFees = json_object_get(pJSON_Root, JSON_INFO_AIRBITZ_FEES_FIELD);
    ABC_CHECK_ASSERT((pJSON_AirBitzFees && json_is_object(pJSON_AirBitzFees)), ABC_CC_JSONError, "Error parsing JSON object value");

    // get the air bitz fees percentage
    pJSON_Value = json_object_get(pJSON_AirBitzFees, JSON_INFO_AIRBITZ_FEE_PERCENTAGE_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_number(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON number value");
    pInfo->pAirBitzFee->percentage = json_number_value(pJSON_Value);

    // get the air bitz fees min satoshi
    pJSON_Value = json_object_get(pJSON_AirBitzFees, JSON_INFO_AIRBITZ_FEE_MIN_SATOSHI_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
    pInfo->pAirBitzFee->minSatoshi = json_integer_value(pJSON_Value);

    // get the air bitz fees max satoshi
    pJSON_Value = json_object_get(pJSON_AirBitzFees, JSON_INFO_AIRBITZ_FEE_MAX_SATOSHI_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
    pInfo->pAirBitzFee->maxSatoshi = json_integer_value(pJSON_Value);

    // get the air bitz fees address
    pJSON_Value = json_object_get(pJSON_AirBitzFees, JSON_INFO_AIRBITZ_FEE_ADDRESS_FIELD);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
    pInfo->pAirBitzFee->szAddresss = stringCopy(json_string_value(pJSON_Value));


    // get the obelisk array
    pJSON_ObeliskArray = json_object_get(pJSON_Root, JSON_INFO_OBELISK_SERVERS_FIELD);
    ABC_CHECK_ASSERT((pJSON_ObeliskArray && json_is_array(pJSON_ObeliskArray)), ABC_CC_JSONError, "Error parsing JSON array value");

    // get the number of elements in the array
    pInfo->countObeliskServers = (unsigned int) json_array_size(pJSON_ObeliskArray);
    if (pInfo->countObeliskServers > 0)
    {
        ABC_ARRAY_NEW(pInfo->aszObeliskServers, pInfo->countObeliskServers, char*);
    }

    // run through all the obelisk servers
    for (unsigned i = 0; i < pInfo->countObeliskServers; i++)
    {
        // get the obelisk server
        pJSON_Value = json_array_get(pJSON_ObeliskArray, i);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
        pInfo->aszObeliskServers[i] = stringCopy(json_string_value(pJSON_Value));
    }

    // get the sync array
    pJSON_SyncArray = json_object_get(pJSON_Root, JSON_INFO_SYNC_SERVERS_FIELD);
    if (pJSON_SyncArray)
    {
        ABC_CHECK_ASSERT((pJSON_SyncArray && json_is_array(pJSON_SyncArray)), ABC_CC_JSONError, "Error parsing JSON array value");

        // get the number of elements in the array
        pInfo->countSyncServers = (unsigned int) json_array_size(pJSON_SyncArray);
        if (pInfo->countSyncServers > 0)
        {
            ABC_ARRAY_NEW(pInfo->aszSyncServers, pInfo->countSyncServers, char*);
        }

        // run through all the sync servers
        for (unsigned i = 0; i < pInfo->countSyncServers; i++)
        {
            // get the sync server
            pJSON_Value = json_array_get(pJSON_SyncArray, i);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
            pInfo->aszSyncServers[i] = stringCopy(json_string_value(pJSON_Value));
        }
    }
    else
    {
        pInfo->countSyncServers = 0;
        pInfo->aszSyncServers = NULL;
    }


    // assign the final result
    *ppInfo = pInfo.release();

exit:
    ABC_FREE_STR(szInfoFilename);

    return cc;
}

/*
 * Gets the general info filename
 *
 * @param pszFilename Location to store allocated filename string (caller must free)
 */
static
tABC_CC ABC_GeneralGetInfoFilename(char **pszFilename,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    std::string filename = gContext->rootDir() + GENERAL_INFO_FILENAME;

    ABC_CHECK_NULL(pszFilename);
    *pszFilename = stringCopy(filename);

exit:
    return cc;
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
    AutoFree<tABC_GeneralInfo, ABC_GeneralFreeInfo> info;
    tABC_Error error;
    if (ABC_CC_Ok == ABC_GeneralGetInfo(&info.get(), &error) &&
        0 < info->countMinersFees)
    {
        BitcoinFeeInfo out;
        for (size_t i = 0; i < info->countMinersFees; ++i)
            out[info->aMinersFees[i]->sizeTransaction] =
                info->aMinersFees[i]->amountSatoshi;
        return out;
    }

    return BitcoinFeeInfo{{0, 10000}};
}

std::vector<std::string>
generalBitcoinServers()
{
    if (isTestnet())
        return std::vector<std::string>{TESTNET_OBELISK};

    AutoFree<tABC_GeneralInfo, ABC_GeneralFreeInfo> info;
    tABC_Error error;
    if (gContext &&
        ABC_CC_Ok == ABC_GeneralGetInfo(&info.get(), &error) &&
        0 < info->countObeliskServers)
    {
        std::vector<std::string> out;
        out.reserve(info->countObeliskServers);
        for (size_t i = 0; i < info->countObeliskServers; ++i)
            out.push_back(info->aszObeliskServers[i]);
        return out;
    }

    return std::vector<std::string>{FALLBACK_OBELISK};
}

std::vector<std::string>
generalSyncServers()
{
    AutoFree<tABC_GeneralInfo, ABC_GeneralFreeInfo> info;
    tABC_Error error;
    if (ABC_CC_Ok == ABC_GeneralGetInfo(&info.get(), &error) &&
        0 < info->countSyncServers)
    {
        std::vector<std::string> out;
        out.reserve(info->countSyncServers);
        for (size_t i = 0; i < info->countSyncServers; ++i)
            out.push_back(info->aszSyncServers[i]);
        return out;
    }

    // Fallback:
    return std::vector<std::string>{
        "https://git3.sync.airbitz.co/repos",
        "https://git4.sync.airbitz.co/repos"
    };
}

} // namespace abcd
