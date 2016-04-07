/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "TxCache.hpp"
#include "../Utility.hpp"
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

// Serialization stuff:
constexpr uint32_t old_serial_magic = 0x3eab61c3; // From the watcher
constexpr uint32_t serial_magic = 0xfecdb763;
constexpr uint8_t serial_tx = 0x42;

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
        for (auto &row: cache_.rows_)
        {
            for (auto &input: row.second.tx.inputs)
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
    problems(bc::hash_digest txid)
    {
        // Just use the previous result if we have been here before:
        auto vi = visited_.find(txid);
        if (visited_.end() != vi)
            return vi->second;

        // We have to assume missing transactions are safe:
        auto i = cache_.rows_.find(txid);
        if (cache_.rows_.end() == i)
            return (visited_[txid] = 0);

        // Confirmed transactions are also safe:
        if (TxState::confirmed == i->second.state)
            return (visited_[txid] = 0);

        // Check for the opt-in replace-by-fee flag:
        unsigned out = 0;
        if (isReplaceByFee(i->second.tx))
            out |= replaceByFee;

        // Recursively check all the inputs:
        for (const auto &input: i->second.tx.inputs)
        {
            out |= problems(input.previous_output.hash);
            if (doubleSpends_.count(input.previous_output))
                out |= doubleSpent;
        }
        return (visited_[txid] = out);
    }

private:
    const TxCache &cache_;

    PointSet spends_;
    PointSet doubleSpends_;
    std::unordered_map<bc::hash_digest, unsigned> visited_;
};

TxCache::~TxCache()
{
}

TxCache::TxCache(unsigned unconfirmed_timeout):
    unconfirmed_timeout_(unconfirmed_timeout)
{
}

Status
TxCache::txidLookup(bc::transaction_type &result, bc::hash_digest txid) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = rows_.find(txid);
    if (rows_.end() == i)
        return ABC_ERROR(ABC_CC_Synchronizing, "Cannot find transaction");

    result = i->second.tx;
    return Status();
}

long long TxCache::txidHeight(bc::hash_digest txid) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = rows_.find(txid);
    if (i == rows_.end())
        return 0;

    if (i->second.state != TxState::confirmed)
    {
        return 0;
    }
    return i->second.block_height;
}

bool
TxCache::isRelevant(const bc::transaction_type &tx,
                    const AddressSet &addresses) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return isRelevantInternal(tx, addresses);
}

bool
TxCache::isRelevantInternal(const bc::transaction_type &tx,
                            const AddressSet &addresses) const
{
    // Scan inputs:
    for (const auto &input: tx.inputs)
    {
        bc::transaction_output_type output;
        auto i = rows_.find(input.previous_output.hash);
        if (rows_.end() != i
                && input.previous_output.index < i->second.tx.outputs.size())
            output = i->second.tx.outputs[input.previous_output.index];

        bc::payment_address address;
        if (bc::extract(address, output.script)
                && addresses.count(address.encoded()))
            return true;
    }

    // Scan outputs:
    for (const auto &output: tx.outputs)
    {
        bc::payment_address address;
        if (bc::extract(address, output.script)
                && addresses.count(address.encoded()))
            return true;
    }

    return false;
}

TxInfo
TxCache::txInfo(const bc::transaction_type &tx) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return txInfoInternal(tx);
}

TxInfo
TxCache::txInfoInternal(const bc::transaction_type &tx) const
{
    TxInfo out;
    int64_t totalIn = 0, totalOut = 0;

    // Basic info:
    out.txid = bc::encode_hash(bc::hash_transaction(tx));
    out.ntxid = bc::encode_hash(makeNtxid(tx));

    // Scan inputs:
    for (const auto &input: tx.inputs)
    {
        bc::transaction_output_type output;
        auto i = rows_.find(input.previous_output.hash);
        if (rows_.end() != i
                && input.previous_output.index < i->second.tx.outputs.size())
            output = i->second.tx.outputs[input.previous_output.index];

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

    return out;
}

Status
TxCache::txidInfo(TxInfo &result, const std::string &txid) const
{
    bc::hash_digest hash;
    if (!bc::decode_hash(hash, txid))
        return ABC_ERROR(ABC_CC_ParseError, "Bad txid");
    bc::transaction_type tx;
    ABC_CHECK(txidLookup(tx, hash));

    result = txInfo(tx);
    return Status();
}

Status
TxCache::txidStatus(TxStatus &result, const std::string &txid) const
{
    bc::hash_digest hash;
    if (!bc::decode_hash(hash, txid))
        return ABC_ERROR(ABC_CC_ParseError, "Bad txid");

    TxStatus out;
    out.height = txidHeight(hash);

    TxGraph graph(*this);
    const auto problems = graph.problems(hash);
    out.isDoubleSpent = problems & TxGraph::doubleSpent;
    out.isReplaceByFee = problems & TxGraph::replaceByFee;

    result = out;
    return Status();
}

std::list<std::pair<TxInfo, TxStatus> >
TxCache::list(const AddressSet &addresses) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::list<std::pair<TxInfo, TxStatus>> out;

    TxGraph graph(*this);
    for (const auto &row: rows_)
    {
        if (isRelevantInternal(row.second.tx, addresses))
        {
            std::pair<TxInfo, TxStatus> pair;
            pair.first = txInfoInternal(row.second.tx);
            pair.second.height = row.second.block_height;
            const auto problems = graph.problems(row.second.txid);
            pair.second.isDoubleSpent = problems & TxGraph::doubleSpent;
            pair.second.isReplaceByFee = problems & TxGraph::replaceByFee;
            out.push_back(pair);
        }
        // TODO: Merge by ntxid
    }

    return out;
}

bool TxCache::has_history(const bc::payment_address &address) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto &row: rows_)
    {
        for (auto &output: row.second.tx.outputs)
        {
            bc::payment_address to_address;
            if (bc::extract(to_address, output.script))
                if (address == to_address)
                    return true;
        }
    }

    return false;
}

bc::output_info_list TxCache::get_utxos(const AddressSet &addresses,
                                        bool filter) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Build a list of spends:
    TxGraph graph(*this);

    // Check each output against the list:
    bc::output_info_list out;
    for (auto &row: rows_)
    {
        for (uint32_t i = 0; i < row.second.tx.outputs.size(); ++i)
        {
            bc::output_point point = {row.first, i};
            const auto &output = row.second.tx.outputs[i];
            bc::payment_address address;

            // The output is interesting if it isn't spent, belongs to us,
            // and its transaction passes the safety checks:
            if (!graph.isSpent(point) &&
                    bc::extract(address, output.script) &&
                    addresses.count(address.encoded()) &&
                    !graph.problems(row.first) &&
                    !(filter && isIncoming(row.second, addresses)))
            {
                bc::output_info_type info = {point, output.value};
                out.push_back(info);
            }
        }
    }

    return out;
}

bc::data_chunk TxCache::serialize() const
{
    ABC_DebugLog("ENTER TxCache::serialize");
    std::lock_guard<std::mutex> lock(mutex_);

    std::basic_ostringstream<uint8_t> stream;
    auto serial = bc::make_serializer(std::ostreambuf_iterator<uint8_t>(stream));

    // Magic version bytes:
    serial.write_4_bytes(serial_magic);

    // Last block height:
    serial.write_8_bytes(0);

    // Tx table:
    time_t now = time(nullptr);
    for (const auto &row: rows_)
    {
        // Don't save old unconfirmed transactions:
        if (row.second.timestamp + unconfirmed_timeout_ < now &&
                TxState::unconfirmed == row.second.state)
        {
            ABC_DebugLog("TxCache::serialize Purging unconfirmed tx");
            continue;
        }

        auto height = row.second.block_height;
        if (TxState::unconfirmed == row.second.state)
            height = row.second.timestamp;

        serial.write_byte(serial_tx);
        serial.write_hash(row.first);
        serial.set_iterator(satoshi_save(row.second.tx, serial.iterator()));
        serial.write_byte(static_cast<uint8_t>(row.second.state));
        serial.write_8_bytes(height);
        serial.write_byte(0); // Was need_check
        serial.write_hash(row.second.txid);
        serial.write_hash(row.second.ntxid);
        serial.write_byte(false); // Was bMalleated
        serial.write_byte(TxState::confirmed == row.second.state); // Was bMasterConfirm
    }

    // The copy is not very elegant:
    auto str = stream.str();
    return bc::data_chunk(str.begin(), str.end());
}

Status
TxCache::load(const bc::data_chunk &data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto serial = bc::make_deserializer(data.begin(), data.end());
    std::unordered_map<bc::hash_digest, TxRow> rows;

    try
    {
        // Header bytes:
        auto magic = serial.read_4_bytes();
        if (serial_magic != magic)
        {
            return old_serial_magic == magic ?
                   ABC_ERROR(ABC_CC_ParseError, "Outdated transaction database format") :
                   ABC_ERROR(ABC_CC_ParseError, "Unknown transaction database header");
        }

        // Last block height:
        (void)serial.read_8_bytes();

        time_t now = time(nullptr);
        while (serial.iterator() != data.end())
        {
            if (serial.read_byte() != serial_tx)
            {
                return ABC_ERROR(ABC_CC_ParseError, "Unknown entry in transaction database");
            }

            bc::hash_digest hash = serial.read_hash();
            TxRow row;
            bc::satoshi_load(serial.iterator(), data.end(), row.tx);
            auto step = serial.iterator() + satoshi_raw_size(row.tx);
            serial.set_iterator(step);

            TxState state      = static_cast<TxState>(serial.read_byte());
            uint64_t height    = serial.read_8_bytes();
            (void)serial.read_byte(); // Was need_check
            row.txid           = serial.read_hash();
            row.ntxid          = serial.read_hash();
            auto malleated     = serial.read_byte();
            auto masterConfirm = serial.read_byte();

            // The height field is the timestamp for unconfirmed txs:
            if (TxState::unconfirmed == row.state)
            {
                row.block_height = 0;
                row.timestamp = height;
            }
            else
            {
                row.block_height = height;
                row.timestamp = now;
            }

            // Malleated transactions can have inaccurate state:
            if (malleated && !masterConfirm)
            {
                row.state = TxState::unconfirmed;
                row.block_height = 0;
            }
            else
            {
                row.state = state;
            }

            rows[hash] = std::move(row);
        }
    }
    catch (bc::end_of_stream)
    {
        return ABC_ERROR(ABC_CC_ParseError, "Truncated transaction database");
    }

    rows_ = rows;
    return Status();
}

void TxCache::dump(std::ostream &out) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto &row: rows_)
    {
        out << "================" << std::endl;
        out << "hash: " << bc::encode_hash(row.first) << std::endl;
        std::string state;
        switch (row.second.state)
        {
        case TxState::unconfirmed:
            out << "state: unconfirmed" << std::endl;
            out << "timestamp: " << row.second.timestamp << std::endl;
            break;
        case TxState::confirmed:
            out << "state: confirmed" << std::endl;
            out << "height: " << row.second.block_height << std::endl;
            break;
        }
        for (auto &input: row.second.tx.inputs)
        {
            bc::payment_address address;
            if (bc::extract(address, input.script))
                out << "input: " << address.encoded() << std::endl;
        }
        for (auto &output: row.second.tx.outputs)
        {
            bc::payment_address address;
            if (bc::extract(address, output.script))
                out << "output: " << address.encoded() << " " <<
                    output.value << std::endl;
        }
    }
}

bool TxCache::insert(const bc::transaction_type &tx)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Do not stomp existing tx's:
    auto txid = bc::hash_transaction(tx);
    if (rows_.find(txid) == rows_.end())
    {
        rows_[txid] = TxRow
        {
            tx, txid, makeNtxid(tx),
            TxState::unconfirmed, 0, time(nullptr)
        };
        return true;
    }

    return false;
}

void
TxCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    rows_.clear();
}

void TxCache::confirmed(bc::hash_digest txid, long long block_height)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rows_.find(txid);
    BITCOIN_ASSERT(it != rows_.end());
    auto &row = it->second;

    row.state = TxState::confirmed;
    row.block_height = block_height;
}

void TxCache::unconfirmed(bc::hash_digest txid)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rows_.find(txid);
    BITCOIN_ASSERT(it != rows_.end());
    auto &row = it->second;

    row.state = TxState::unconfirmed;
    row.block_height = 0;
}

void TxCache::reset_timestamp(bc::hash_digest txid)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = rows_.find(txid);
    if (i != rows_.end())
        i->second.timestamp = time(nullptr);
}

void TxCache::foreach_unconfirmed(HashFn &&f)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto row: rows_)
        if (row.second.state != TxState::confirmed)
            f(row.first);
}

bool
TxCache::isIncoming(const TxRow &row,
                    const AddressSet &addresses) const
{
    // Confirmed transactions are no longer incoming:
    if (TxState::confirmed == row.state)
        return false;

    // This is a spend if we control all the inputs:
    for (auto &input: row.tx.inputs)
    {
        bc::payment_address address;
        if (!bc::extract(address, input.script) ||
                !addresses.count(address.encoded()))
            return true;
    }
    return false;
}

} // namespace abcd
