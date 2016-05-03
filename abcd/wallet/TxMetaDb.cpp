/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "TxMetaDb.hpp"
#include "Wallet.hpp"
#include "../crypto/Crypto.hpp"
#include "../json/JsonObject.hpp"
#include "../util/Debug.hpp"
#include "../util/FileIO.hpp"
#include <dirent.h>

namespace abcd {

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
    ABC_JSON_VALUE(metadata, "meta", JsonPtr)

    Status
    pack(const TxMeta &in);

    Status
    unpack(TxMeta &result);
};

Status
TxJson::pack(const TxMeta &in)
{
    // Main json:
    ABC_CHECK(ntxidSet(in.ntxid));

    // State json:
    TxStateJson stateJson;
    ABC_CHECK(stateJson.txidSet(in.txid));
    ABC_CHECK(stateJson.timeCreationSet(in.timeCreation));
    ABC_CHECK(stateJson.internalSet(in.internal));
    ABC_CHECK(stateSet(stateJson));

    // Details json:
    JsonPtr metaJson;
    ABC_CHECK(in.metadata.save(metaJson));
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
    ABC_CHECK(out.metadata.load(metadata()));

    result = std::move(out);
    return Status();
}

TxMetaDb::TxMetaDb(const Wallet &wallet):
    wallet_(wallet),
    dir_(wallet.paths.txsDir())
{
}

Status
TxMetaDb::load()
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
TxMetaDb::save(const TxMeta &tx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    txs_[tx.ntxid] = tx;

    ABC_CHECK(fileEnsureDir(dir_));
    TxJson json(files_[tx.ntxid]);
    if (!json)
        json = JsonObject();
    ABC_CHECK(json.pack(tx));
    ABC_CHECK(json.save(path(tx), wallet_.dataKey()));
    files_[tx.ntxid] = json;

    return Status();
}

Status
TxMetaDb::get(TxMeta &result, const std::string &ntxid)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = txs_.find(ntxid);
    if (i == txs_.end())
        return ABC_ERROR(ABC_CC_NoTransaction, "No transaction: " + ntxid);

    result = i->second;
    return Status();
}

std::string
TxMetaDb::path(const TxMeta &tx)
{
    return dir_ + cryptoFilename(wallet_.dataKey(), tx.ntxid) +
           (tx.internal ? "-int.json" : "-ext.json");
}

}
