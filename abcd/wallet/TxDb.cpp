/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "TxDb.hpp"
#include "Wallet.hpp"
#include "../crypto/Crypto.hpp"
#include "../json/JsonObject.hpp"
#include "../util/Debug.hpp"
#include "../util/FileIO.hpp"
#include <dirent.h>

namespace abcd {

struct TxMetaJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(TxMetaJson, JsonObject)

    ABC_JSON_INTEGER(airbitzFeeSent, "amountFeeAirBitzSatoshi", 0);
    ABC_JSON_INTEGER(balance, "amountSatoshi", 0);
    ABC_JSON_INTEGER(fee, "amountFeeMinersSatoshi", 0);
};

struct TxStateJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(TxStateJson, JsonObject)

    ABC_JSON_STRING(txid, "malleableTxId", "") // Optional
    ABC_JSON_INTEGER(timeCreation, "creationDate", 0)
    ABC_JSON_BOOLEAN(internal, "internal", false)
};

struct TxJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(TxJson, JsonObject)

    ABC_JSON_STRING(ntxid, "ntxid", nullptr)
    ABC_JSON_VALUE(state, "state", TxStateJson)
    ABC_JSON_VALUE(metadata, "meta", TxMetaJson)
    ABC_JSON_INTEGER(airbitzFeeWanted, "airbitzFeeWanted", 0);

    Status
    pack(const TxMeta &in, int64_t balance, int64_t fee);

    Status
    unpack(TxMeta &result);
};

Status
TxJson::pack(const TxMeta &in, int64_t balance, int64_t fee)
{
    // Main json:
    ABC_CHECK(ntxidSet(in.ntxid));
    ABC_CHECK(airbitzFeeWantedSet(in.airbitzFeeWanted));

    // State json:
    TxStateJson stateJson;
    ABC_CHECK(stateJson.txidSet(in.txid));
    ABC_CHECK(stateJson.timeCreationSet(in.timeCreation));
    ABC_CHECK(stateJson.internalSet(in.internal));
    ABC_CHECK(stateSet(stateJson));

    // Details json:
    TxMetaJson metaJson;
    ABC_CHECK(in.metadata.save(metaJson));
    ABC_CHECK(metaJson.airbitzFeeSentSet(in.airbitzFeeSent));
    ABC_CHECK(metaJson.balanceSet(balance));
    ABC_CHECK(metaJson.feeSet(fee));
    ABC_CHECK(metadataSet(metaJson));

    return Status();
}

Status
TxJson::unpack(TxMeta &result)
{
    TxMeta out;

    // Main json:
    ABC_CHECK(ntxidOk());
    out.ntxid = ntxid();

    // State json:
    TxStateJson stateJson = state();
    out.txid = stateJson.txid();
    out.timeCreation = stateJson.timeCreation();
    out.internal = stateJson.internal();

    // Details json:
    auto metaJson = metadata();
    ABC_CHECK(out.metadata.load(metaJson));
    out.airbitzFeeSent = metaJson.airbitzFeeSent();

    if (airbitzFeeWantedOk())
        out.airbitzFeeWanted = airbitzFeeWanted();
    else
        out.airbitzFeeWanted = out.airbitzFeeSent;

    result = std::move(out);
    return Status();
}

TxDb::TxDb(const Wallet &wallet):
    wallet_(wallet),
    dir_(wallet.paths.txsDir())
{
}

Status
TxDb::load()
{
    std::lock_guard<std::mutex> lock(mutex_);

    txs_.clear();
    files_.clear();

    // Open the directory:
    DIR *dir = opendir(dir_.c_str());
    if (dir)
    {
        struct dirent *de;
        while (nullptr != (de = readdir(dir)))
        {
            if (!fileIsJson(de->d_name))
                continue;

            // Try to load the address:
            TxMeta tx;
            TxJson json;
            if (json.load(dir_ + de->d_name, wallet_.dataKey()).log() &&
                    json.unpack(tx).log())
            {
                if (path(tx) != dir_ + de->d_name)
                    ABC_DebugLog("Filename %s does not match transaction", de->d_name);

                // Delete duplicate transactions, if any:
                auto i = txs_.find(tx.ntxid);
                if (i != txs_.end())
                {
                    if (tx.internal)
                        fileDelete(path(i->second)).log();
                    else
                        fileDelete(dir_ + de->d_name).log();
                }

                // Save this transaction if is unique or internal:
                if (i == txs_.end() || tx.internal)
                {
                    txs_[tx.ntxid] = tx;
                    files_[tx.ntxid] = json;
                }
            }
        }
        closedir(dir);
    }

    return Status();
}

Status
TxDb::save(const TxMeta &tx, int64_t balance, int64_t fee)
{
    std::lock_guard<std::mutex> lock(mutex_);

    txs_[tx.ntxid] = tx;

    ABC_CHECK(fileEnsureDir(dir_));
    TxJson json(files_[tx.ntxid]);
    if (!json)
        json = JsonObject();
    ABC_CHECK(json.pack(tx, balance, fee));
    ABC_CHECK(json.save(path(tx), wallet_.dataKey()));
    files_[tx.ntxid] = json;

    return Status();
}

Status
TxDb::get(TxMeta &result, const std::string &ntxid)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = txs_.find(ntxid);
    if (i == txs_.end())
        return ABC_ERROR(ABC_CC_NoTransaction, "No transaction: " + ntxid);

    result = i->second;
    return Status();
}

int64_t
TxDb::airbitzFeePending()
{
    std::lock_guard<std::mutex> lock(mutex_);

    int64_t totalWanted = 0;
    int64_t totalSent = 0;
    for (const auto &i: txs_)
    {
        totalWanted += i.second.airbitzFeeWanted;
        totalSent += i.second.airbitzFeeSent;
    }

    return totalWanted - totalSent;
}

time_t
TxDb::airbitzFeeLastSent()
{
    std::lock_guard<std::mutex> lock(mutex_);
    time_t out = 0;

    for (const auto &i: txs_)
        if (i.second.airbitzFeeSent && out < i.second.timeCreation)
            out = i.second.timeCreation;

    return out;
}

std::string
TxDb::path(const TxMeta &tx)
{
    return dir_ + cryptoFilename(wallet_.dataKey(), tx.ntxid) +
           (tx.internal ? "-int.json" : "-ext.json");
}

}
