/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "TxCache.hpp"
#include "BlockCache.hpp"
#include "../Utility.hpp"
#include "../../crypto/Encoding.hpp"
#include "../../json/JsonArray.hpp"
#include "../../json/JsonObject.hpp"
#include "../../util/Debug.hpp"
#include <unordered_set>

namespace std {

/**
 * Allows `bc::point_type` to be used with `std::unordered_set`.
 */
template<> struct hash<bc::point_type>
{
    typedef bc::point_type argument_type;
    typedef std::size_t result_type;

    result_type
    operator()(argument_type const &p) const
    {
        auto h = libbitcoin::from_little_endian_unsafe<result_type>(
                     p.hash.begin());
        return h ^ p.index;
    }
};

} // namespace std

namespace abcd {

libbitcoin::output_info_list
filterOutputs(const TxOutputList &utxos, bool filter)
{
    libbitcoin::output_info_list out;
    out.reserve(utxos.size());
    for (const auto &utxo: utxos)
    {
        if (utxo.isSpendable && (!filter || !utxo.isIncoming))
        {
            out.push_back(libbitcoin::output_info_type
            {
                utxo.point, utxo.value
            });
        }
    }
    return out;
}

typedef std::unordered_set<bc::point_type> PointSet;

/**
 * Knows how to check a transaction for double-spends and other problems.
 * This uses a memoized recursive function to do the graph search,
 * so the more checks this object performs,
 * the faster those checks can potentially become (for a fixed graph).
 */
class TxGraph
{
public:
    static constexpr unsigned doubleSpent = 1 << 0;
    static constexpr unsigned replaceByFee = 1 << 1;

    TxGraph(const TxCache &cache):
        cache_(cache)
    {
        for (auto &row: cache_.txs_)
        {
            for (auto &input: row.second.inputs)
            {
                if (!spends_.insert(input.previous_output).second)
                    doubleSpends_.insert(input.previous_output);
            }
        }
    }

    /**
     * Returns true if the output point has been spent from.
     */
    bool isSpent(bc::output_point point)
    {
        return spends_.count(point);
    }

    /**
     * Recursively checks the transaction graph for problems.
     * @return A bitfield containing problem flags.
     */
    unsigned
    problems(const std::string &txid)
    {
        // Just use the previous result if we have been here before:
        auto vi = visited_.find(txid);
        if (visited_.end() != vi)
            return vi->second;

        // We have to assume missing transactions are safe:
        auto i = cache_.txs_.find(txid);
        if (cache_.txs_.end() == i)
            return (visited_[txid] = 0);

        // Confirmed transactions are also safe:
        if (cache_.txidHeight(txid))
            return (visited_[txid] = 0);

        // Check for the opt-in replace-by-fee flag:
        unsigned out = 0;
        if (isReplaceByFee(i->second))
            out |= replaceByFee;

        // Recursively check all the inputs:
        for (const auto &input: i->second.inputs)
        {
            out |= problems(bc::encode_hash(input.previous_output.hash));
            if (doubleSpends_.count(input.previous_output))
                out |= doubleSpent;
        }
        return (visited_[txid] = out);
    }

private:
    const TxCache &cache_;

    PointSet spends_;
    PointSet doubleSpends_;
    std::map<std::string, unsigned> visited_;
};

struct CacheJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(CacheJson, JsonObject)

    ABC_JSON_VALUE(txs, "txs", JsonArray)
    ABC_JSON_VALUE(heights, "heights", JsonArray)
};

struct TxJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(TxJson, JsonObject)

    ABC_JSON_STRING(txid, "txid", 0)
    ABC_JSON_STRING(data, "data", 0)
};

struct HeightJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(HeightJson, JsonObject)

    ABC_JSON_STRING(txid, "txid", 0)
    ABC_JSON_INTEGER(height, "height", 0)
    ABC_JSON_INTEGER(firstSeen, "firstSeen", 0)
};


TxCache::TxCache(BlockCache &blockCache):
    blocks_(blockCache)
{
}

void
TxCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    txs_.clear();
    heights_.clear();
}

Status
TxCache::load(JsonObject &json)
{
    std::lock_guard<std::mutex> lock(mutex_);
    CacheJson cacheJson(json);

    // Tx data:
    auto txsJson = cacheJson.txs();
    size_t txsSize = txsJson.size();
    for (size_t i = 0; i < txsSize; i++)
    {
        TxJson txJson(txsJson[i]);
        if (txJson.txidOk() && txJson.dataOk())
        {
            DataChunk rawTx;
            ABC_CHECK(base64Decode(rawTx, txJson.data()));
            bc::transaction_type tx;
            ABC_CHECK(decodeTx(tx, rawTx));

            txs_[txJson.txid()] = std::move(tx);
        }
    }

    // Heights:
    auto heightsJson = cacheJson.heights();
    size_t heightsSize = heightsJson.size();
    for (size_t i = 0; i < heightsSize; i++)
    {
        HeightJson heightJson(heightsJson[i]);
        if (heightJson.txidOk())
        {
            HeightInfo info;
            info.height = heightJson.height();
            info.firstSeen = heightJson.firstSeen();
            heights_[heightJson.txid()] = info;
            blocks_.headerNeededAdd(info.height);
        }
    }

    return Status();
}

Status
TxCache::save(JsonObject &json)
{
    std::lock_guard<std::mutex> lock(mutex_);
    CacheJson cacheJson(json);

    // Tx data:
    JsonArray txsJson;
    for (const auto &tx: txs_)
    {
        bc::data_chunk rawTx(satoshi_raw_size(tx.second));
        bc::satoshi_save(tx.second, rawTx.begin());

        TxJson txJson;
        ABC_CHECK(txJson.txidSet(tx.first));
        ABC_CHECK(txJson.dataSet(base64Encode(rawTx)));
        ABC_CHECK(txsJson.append(txJson));
    }
    cacheJson.txsSet(txsJson);

    // Heights:
    JsonArray heightsJson;
    for (const auto &height: heights_)
    {
        HeightJson heightJson;
        ABC_CHECK(heightJson.txidSet(height.first));
        if (height.second.height)
            ABC_CHECK(heightJson.heightSet(height.second.height));
        ABC_CHECK(heightJson.firstSeenSet(height.second.firstSeen));
        ABC_CHECK(heightsJson.append(heightJson));
    }
    cacheJson.heightsSet(heightsJson);

    return Status();
}

Status
TxCache::get(bc::transaction_type &result, const std::string &txid) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = txs_.find(txid);
    if (txs_.end() == i)
        return ABC_ERROR(ABC_CC_Synchronizing, "Cannot find transaction");

    result = i->second;
    return Status();
}

Status
TxCache::info(TxInfo &result, const bc::transaction_type &tx) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    ABC_CHECK(infoInternal(result, tx));
    return Status();
}

Status
TxCache::info(TxInfo &result, const std::string &txid) const
{
    bc::transaction_type tx;
    ABC_CHECK(get(tx, txid));
    ABC_CHECK(info(result, tx));
    return Status();
}

Status
TxCache::infoInternal(TxInfo &result, const bc::transaction_type &tx) const
{
    TxInfo out;
    int64_t totalIn = 0, totalOut = 0;

    // Basic info:
    out.txid = bc::encode_hash(bc::hash_transaction(tx));
    out.ntxid = bc::encode_hash(makeNtxid(tx));

    // Scan inputs:
    for (const auto &input: tx.inputs)
    {
        const auto txid = bc::encode_hash(input.previous_output.hash);
        auto i = txs_.find(txid);
        if (txs_.end() == i)
            break;
            // return ABC_ERROR(ABC_CC_Synchronizing, "Missing input " + txid);
        if (i->second.outputs.size() <= input.previous_output.index)
            return ABC_ERROR(ABC_CC_Error, "Impossible input on " + txid);
        auto &output = i->second.outputs[input.previous_output.index];

        totalIn += output.value;
        bc::payment_address address;
        bc::extract(address, output.script);
        out.ios.push_back(TxInOut{true, output.value, address.encoded()});
    }

    // Scan outputs:
    for (const auto &output: tx.outputs)
    {
        totalOut += output.value;
        bc::payment_address address;
        bc::extract(address, output.script);
        out.ios.push_back(TxInOut{false, output.value, address.encoded()});
    }

    out.fee = totalIn - totalOut;

    result = out;
    return Status();
}

bool
TxCache::missing(const std::string &txid) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Check the transaction:
    auto i = txs_.find(txid);
    if (txs_.end() == i)
        return true;

    // Check the inputs:
    // for (const auto &input: i->second.inputs)
    // {
    //     const auto txid = bc::encode_hash(input.previous_output.hash);
    //     if (!txs_.count(txid))
    //         return true;
    // }

    return false;
}

TxidSet
TxCache::missingTxids(const TxidSet &txids) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    TxidSet out;

    for (const auto &txid: txids)
    {
        // Check the transaction:
        auto i = txs_.find(txid);
        if (txs_.end() == i)
        {
            out.insert(txid);
            continue;
        }

        // Check the inputs:
        // for (const auto &input: i->second.inputs)
        // {
        //     const auto txid = bc::encode_hash(input.previous_output.hash);
        //     if (!txs_.count(txid))
        //         out.insert(txid);
        // }
    }

    return out;
}

Status
TxCache::status(TxStatus &result, const std::string &txid) const
{
    TxGraph graph(*this);
    TxStatus out;
    out.height = txidHeight(txid);
    const auto problems = graph.problems(txid);
    out.isDoubleSpent = problems & TxGraph::doubleSpent;
    out.isReplaceByFee = problems & TxGraph::replaceByFee;

    result = out;
    return Status();
}

std::list<std::pair<TxInfo, TxStatus> >
TxCache::statuses(const TxidSet &txids) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::list<std::pair<TxInfo, TxStatus>> out;

    TxGraph graph(*this);
    for (const auto &txid: txids)
    {
        auto i = txs_.find(txid);
        std::pair<TxInfo, TxStatus> pair;
        if (txs_.end() != i && infoInternal(pair.first, i->second))
        {
            pair.second.height = txidHeight(i->first);
            const auto problems = graph.problems(i->first);
            pair.second.isDoubleSpent = problems & TxGraph::doubleSpent;
            pair.second.isReplaceByFee = problems & TxGraph::replaceByFee;
            out.push_back(pair);
        }
    }

    // TODO: Merge by ntxid

    return out;
}

TxOutputList
TxCache::utxos(const AddressSet &addresses) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Build a list of spends:
    TxGraph graph(*this);

    // Check each output against the list:
    TxOutputList out;
    for (auto &row: txs_)
    {
        for (uint32_t i = 0; i < row.second.outputs.size(); ++i)
        {
            bc::hash_digest hash;
            bc::decode_hash(hash, row.first);

            bc::output_point point = {hash, i};
            const auto &output = row.second.outputs[i];
            bc::payment_address address;
            const auto txid = row.first;

            // The output is interesting if it isn't spent and belongs to us:
            if (!graph.isSpent(point) &&
                    bc::extract(address, output.script) &&
                    addresses.count(address.encoded()))
            {
                out.push_back(TxOutput
                {
                    point, output.value,
                    !graph.problems(row.first),
                    isIncoming(row.second, txid, addresses)
                });
            }
        }
    }

    return out;
}

bool
TxCache::drop(const std::string &txid, time_t now)
{
    std::unique_lock<std::mutex> lock(mutex_);

    // Do not drop if it is confirmed or less than an hour old:
    const auto &info = heights_[txid];
    if (info.height || now < info.firstSeen + 60*60)
        return false;

    heights_.erase(txid);
    txs_.erase(txid);
    return true;
}

bool
TxCache::insert(const bc::transaction_type &tx)
{
    std::unique_lock<std::mutex> lock(mutex_);

    // Do not stomp existing tx's:
    auto txid = bc::encode_hash(bc::hash_transaction(tx));
    if (txs_.find(txid) == txs_.end())
    {
        txs_[txid] = tx;
        return true;
    }

    return false;
}

void
TxCache::confirmed(const std::string &txid, size_t height, time_t now)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto &info = heights_[txid];
    info.height = height;
    blocks_.headerNeededAdd(height);
    if (0 == info.firstSeen)
        info.firstSeen = now;
}

bool
TxCache::isIncoming(const bc::transaction_type &tx, const std::string &txid,
                    const AddressSet &addresses) const
{
    // Confirmed transactions are no longer incoming:
    if (txidHeight(txid))
        return false;

    // This is a spend if we control all the inputs:
    for (auto &input: tx.inputs)
    {
        bc::payment_address address;
        if (!bc::extract(address, input.script) ||
                !addresses.count(address.encoded()))
            return true;
    }
    return false;
}

size_t
TxCache::txidHeight(const std::string &txid) const
{
    const auto i = heights_.find(txid);
    if (heights_.end() == i)
        return 0;
    return i->second.height;
}

} // namespace abcd
