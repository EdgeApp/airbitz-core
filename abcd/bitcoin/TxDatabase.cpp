/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */

#include "TxDatabase.hpp"
#include "WatcherBridge.hpp"
#include "../util/Debug.hpp"

namespace abcd {

// Serialization stuff:
constexpr uint32_t old_serial_magic = 0x3eab61c3; // From the watcher
constexpr uint32_t serial_magic = 0xfecdb763;
constexpr uint8_t serial_tx = 0x42;

static bc::hash_digest
get_non_malleable_txid(bc::transaction_type tx)
{
    for (auto &input: tx.inputs)
        input.script = bc::script_type();
    return bc::hash_transaction(tx, bc::sighash::all);
}

TxDatabase::~TxDatabase()
{
}

TxDatabase::TxDatabase(unsigned unconfirmed_timeout):
    last_height_(0),
    unconfirmed_timeout_(unconfirmed_timeout)
{
}

long long TxDatabase::last_height() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return last_height_;
}

bool TxDatabase::txidExists(bc::hash_digest txid) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    return rows_.find(txid) != rows_.end();
}

bool TxDatabase::ntxidExists(bc::hash_digest ntxid)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto txRows = ntxidLookupAll(ntxid);
    return !txRows.empty();
}

bc::transaction_type TxDatabase::txidLookup(bc::hash_digest txid) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = rows_.find(txid);
    if (i == rows_.end())
        return bc::transaction_type();
    return i->second.tx;
}

bc::transaction_type TxDatabase::ntxidLookup(bc::hash_digest ntxid)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<TxRow *> txRows = ntxidLookupAll(ntxid);

    bc::transaction_type tx;
    bool foundTx = false;

    for (auto i = txRows.begin(); i != txRows.end(); ++i) {
        // Try to return the master confirmed txid if possible
        // Otherwise return any confirmed txid
        // Otherwise return any match
        if (!foundTx) {
            tx = (*i)->tx;
            foundTx = true;
        } else {
            if (TxState::confirmed == (*i)->state) {
                tx = (*i)->tx;
            }
        }

        if ((*i)->bMasterConfirm)
            return tx;
    }
    return tx;
}

static const char unsent[] = "unsent";
static const char unconfirmed[] = "unconfirmed";
static const char confirmed[] = "confirmed";

const char * stateToString(TxState state)
{
    switch (state) {
        case TxState::unsent:
            return unsent;
        case TxState::unconfirmed:
            return unconfirmed;
        case TxState::confirmed:
            return confirmed;
    }

}

long long TxDatabase::txidHeight(bc::hash_digest txid) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = rows_.find(txid);
    if (i == rows_.end())
        return 0;

    if (i->second.state != TxState::confirmed) {
        return 0;
    }
    return i->second.block_height;
}

long long TxDatabase::ntxidHeight(bc::hash_digest ntxid)
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<TxRow *> txRows = ntxidLookupAll(ntxid);
    if (txRows.empty())
        return NTXID_HEIGHT_NOT_FOUND;

    long long height = 0;
    for (auto row: txRows)
    {
        if (TxState::confirmed == row->state)
        {
            if (height < row->block_height)
                height = row->block_height;
        }
    }

    // Special signal to the GUI that the transaction is both
    // malleated and unconfirmed:
    if (1 < txRows.size() && !height)
        return -1;

    return height;
}

bool TxDatabase::has_history(const bc::payment_address &address) const
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

bc::output_info_list TxDatabase::get_utxos() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Allow inserting bc::output_point into std::set:
    class point_cmp {
    public:
        bool operator () (const bc::output_point &a, const bc::output_point &b)
        {
            if (a.hash == b.hash)
                return a.index < b.index;
            else
                return a.hash < b.hash;
        }
    };

    // Build a list of spent outputs:
    std::set<bc::output_point, point_cmp> spends;
    for (auto &row: rows_)
        for (auto &input: row.second.tx.inputs)
            spends.insert(input.previous_output);

    // Check each output against the list:
    bc::output_info_list out;
    for (auto &row: rows_)
    {
        // Exclude unconfirmed malleated transactions from UTXO set.
        if ((false == row.second.bMalleated) || (TxState::confirmed == row.second.state && row.second.bMasterConfirm)) {
            for (uint32_t i = 0; i < row.second.tx.outputs.size(); ++i)
            {
                auto &output = row.second.tx.outputs[i];
                bc::output_point point = {row.first, i};
                if (spends.find(point) == spends.end())
                {
                    bc::output_info_type info = {point, output.value};
                    out.push_back(info);
                }
            }
        }
    }
    return out;
}

bc::output_info_list TxDatabase::get_utxos(const AddressSet &addresses,
    bool filter) const
{
    auto raw = get_utxos();

    std::lock_guard<std::mutex> lock(mutex_);
    bc::output_info_list utxos;
    for (auto &utxo: raw)
    {
        auto i = rows_.find(utxo.point.hash);
        BITCOIN_ASSERT(i != rows_.end());
        const auto &tx = i->second.tx;
        auto &output = tx.outputs[utxo.point.index];

        bc::payment_address to_address;
        if (bc::extract(to_address, output.script))
            if (addresses.find(to_address) != addresses.end())
                utxos.push_back(utxo);
    }

    // Filter out unconfirmed ones:
    if (filter)
    {
        bc::output_info_list out;
        for (auto &utxo: utxos)
        {
            if (isSpendable(utxo.point.hash, addresses))
                out.push_back(utxo);
        }
        utxos = std::move(out);
    }

    return utxos;
}

bc::data_chunk TxDatabase::serialize() const
{
    ABC_DebugLog("ENTER TxDatabase::serialize");
    std::lock_guard<std::mutex> lock(mutex_);

    std::basic_ostringstream<uint8_t> stream;
    auto serial = bc::make_serializer(std::ostreambuf_iterator<uint8_t>(stream));

    // Magic version bytes:
    serial.write_4_bytes(serial_magic);

    // Last block height:
    serial.write_8_bytes(last_height_);

    // Tx table:
    time_t now = time(nullptr);
    for (const auto &row: rows_)
    {
        // Don't save old unconfirmed transactions:
        if (row.second.timestamp + unconfirmed_timeout_ < now &&
                TxState::unconfirmed == row.second.state) {
            ABC_DebugLog("TxDatabase::serialize Purging unconfirmed tx");
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
        serial.write_byte(row.second.need_check);
        serial.write_hash(row.second.txid);
        serial.write_hash(row.second.ntxid);
        serial.write_byte(row.second.bMalleated);
        serial.write_byte(row.second.bMasterConfirm);

    }

    // The copy is not very elegant:
    auto str = stream.str();
    return bc::data_chunk(str.begin(), str.end());
}

bool TxDatabase::load(const bc::data_chunk &data)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto serial = bc::make_deserializer(data.begin(), data.end());
    size_t last_height;
    std::unordered_map<bc::hash_digest, TxRow> rows;

    try
    {
        // Header bytes:
        auto magic = serial.read_4_bytes();
        if (old_serial_magic == magic)
        {
            ABC_DebugLog("TxDatabase::load old_serial_magic != magic - return true");
            return true;
        }
        if (serial_magic != magic)
        {
            ABC_DebugLog("TxDatabase::load serial_magic != magic - return false");
            return false;
        }

        // Last block height:
        last_height = serial.read_8_bytes();

        time_t now = time(nullptr);
        while (serial.iterator() != data.end())
        {
            if (serial.read_byte() != serial_tx)
            {
                ABC_DebugLog("TxDatabase::load serial.read_byte() != serial_tx - return false");
                return false;
            }

            bc::hash_digest hash = serial.read_hash();
            TxRow row;
            bc::satoshi_load(serial.iterator(), data.end(), row.tx);
            auto step = serial.iterator() + satoshi_raw_size(row.tx);
            serial.set_iterator(step);
            row.state = static_cast<TxState>(serial.read_byte());
            row.block_height = serial.read_8_bytes();
            row.timestamp = now;
            if (TxState::unconfirmed == row.state)
                row.timestamp = row.block_height;
            row.need_check = serial.read_byte();

            row.txid           = serial.read_hash();
            row.ntxid          = serial.read_hash();
            row.bMalleated     = serial.read_byte();
            row.bMasterConfirm = serial.read_byte();

            rows[hash] = std::move(row);
        }
    }
    catch (bc::end_of_stream)
    {
        ABC_DebugLog("TxDatabase::load try->catch() - return false");
        return false;
    }
    last_height_ = last_height;
    rows_ = rows;
    ABC_DebugLog("TxDatabase::load last_eight=%d - return true", last_height);
    return true;
}

void TxDatabase::dump(std::ostream &out) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    out << "height: " << last_height_ << std::endl;
    for (const auto &row: rows_)
    {
        out << "================" << std::endl;
        out << "hash: " << bc::encode_hash(row.first) << std::endl;
        std::string state;
        switch (row.second.state)
        {
        case TxState::unsent:
            out << "state: unsent" << std::endl;
            break;
        case TxState::unconfirmed:
            out << "state: unconfirmed" << std::endl;
            out << "timestamp: " << row.second.timestamp << std::endl;
            break;
        case TxState::confirmed:
            out << "state: confirmed" << std::endl;
            out << "height: " << row.second.block_height << std::endl;
            if (row.second.need_check)
                out << "needs check." << std::endl;
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

std::vector<TxDatabase::TxRow *>
TxDatabase::ntxidLookupAll(bc::hash_digest ntxid)
{
    std::vector<TxRow *> out;
    for (auto &row: rows_)
    {
        if (row.second.ntxid == ntxid)
            out.push_back(&row.second);
    }
    return out;
}

bool TxDatabase::insert(const bc::transaction_type &tx, TxState state)
{

    std::lock_guard<std::mutex> lock(mutex_);

    //
    // Yuck. Maybe this func should be in transaction.cpp
    //
    bc::hash_digest ntxid = get_non_malleable_txid(tx);

    // Do not stomp existing tx's:
    auto txid = bc::hash_transaction(tx);
    if (rows_.find(txid) == rows_.end()) {

        TxState state = TxState::unconfirmed;
        long long height = 0;
        bool bMalleated = false;

        // Check if there are other transactions with same txid.
        // If so, mark all malleated and copy block height and state to
        // new tx
        std::vector<TxRow *> txRows = ntxidLookupAll(ntxid);

        for (auto i = txRows.begin(); i != txRows.end(); ++i) {
            if (txid != (*i)->txid) {
                height = (*i)->block_height;
                state = (*i)->state;
                bMalleated = (*i)->bMalleated = true;
            }
        }

        rows_[txid] = TxRow{tx, txid, ntxid, state, height, time(nullptr), bMalleated, false, false};
        return true;
    }

    return false;

}

void TxDatabase::at_height(size_t height)
{
    std::lock_guard<std::mutex> lock(mutex_);
    last_height_ = height;

    // Check for blockchain forks:
    check_fork(height);
}

void TxDatabase::confirmed(bc::hash_digest txid, long long block_height)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rows_.find(txid);
    BITCOIN_ASSERT(it != rows_.end());
    auto &row = it->second;

    // If the transaction was already confirmed in another block,
    // that means the chain has forked:
    if (row.state == TxState::confirmed && row.block_height != block_height)
    {
        //on_fork_();
        check_fork(row.block_height);
    }

    // Check if there are other malleated transactions.
    // If so, mark them all confirmed
    std::vector<TxRow *> txRows = ntxidLookupAll(it->second.ntxid);

    row.state = TxState::confirmed;
    row.block_height = block_height;
    row.bMasterConfirm = true;

    for (auto i = txRows.begin(); i != txRows.end(); ++i)
    {
        if (txid != (*i)->txid)
        {
            (*i)->block_height = block_height;
            (*i)->state = TxState::confirmed;
            (*i)->bMalleated = true;
            row.bMalleated = true;
        }
    }
}

void TxDatabase::unconfirmed(bc::hash_digest txid)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = rows_.find(txid);
    BITCOIN_ASSERT(it != rows_.end());
    auto &row = it->second;

    long long height = 0;
    bool bMalleated  = row.bMalleated;
    TxState state = TxState::unconfirmed;

    // If the transaction was already confirmed, and is now unconfirmed,
    // we probably have a block fork:
//    std::string malTxID1 = bc::encode_hash(bc::hash_transaction(row.tx));

    if (row.state == TxState::confirmed) {

        // Check if there are other malleated transactions.
        // If so, mark them all unconfirmed_malleated
        std::vector<TxRow *> txRows = ntxidLookupAll(it->second.ntxid);

        for (auto i = txRows.begin(); i != txRows.end(); ++i) {
            if (txid != (*i)->txid)
            {
                if ((*i)->bMasterConfirm)
                {
                    height = (*i)->block_height;
                    state = (*i)->state;
                }
                else
                {
                    (*i)->block_height = height = -1;
                    (*i)->state = TxState::unconfirmed;
                    (*i)->bMalleated = bMalleated = true;
                }
            }
        }

        if (TxState::unconfirmed == row.state)
            check_fork(row.block_height);
    }

    row.block_height = height;
    row.state = state;
    row.bMalleated = bMalleated;
}

void TxDatabase::forget(bc::hash_digest txid)
{
    std::lock_guard<std::mutex> lock(mutex_);

    rows_.erase(txid);
}

void TxDatabase::reset_timestamp(bc::hash_digest txid)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = rows_.find(txid);
    if (i != rows_.end())
        i->second.timestamp = time(nullptr);
}

void TxDatabase::foreach_unconfirmed(HashFn &&f)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto row: rows_)
        if (row.second.state != TxState::confirmed)
            f(row.first);
}

void TxDatabase::foreach_forked(HashFn &&f)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto row: rows_)
        if (row.second.state == TxState::confirmed && row.second.need_check)
            f(row.first);
}

void TxDatabase::foreach_unsent(TxFn &&f)
{
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto row: rows_)
        if (row.second.state == TxState::unsent)
            f(row.second.tx);
}

/**
 * It is possible that the blockchain has forked. Therefore, mark all
 * transactions just below the given height as needing to be checked.
 */
void TxDatabase::check_fork(size_t height)
{
    // Find the height of next-lower block that has transactions in it:
    size_t prev_height = 0;
    for (auto row: rows_)
        if (row.second.state == TxState::confirmed &&
            row.second.block_height < height &&
            prev_height < row.second.block_height)
            prev_height = row.second.block_height;

    // Mark all transactions at that level as needing checked:
    for (auto row: rows_)
        if (row.second.state == TxState::confirmed &&
            row.second.block_height == prev_height)
            row.second.need_check = true;
}

bool
TxDatabase::isSpendable(bc::hash_digest txid, const AddressSet &addresses) const
{
    auto i = rows_.find(txid);
    if (i == rows_.end())
        return false;

    if (TxState::confirmed == i->second.state)
        return true;

    // This is a spend if we control all the inputs:
    for (auto &input: i->second.tx.inputs)
    {
        bc::payment_address address;
        if (!bc::extract(address, input.script))
            return false;
        if (addresses.find(address) == addresses.end())
            return false;
    }
    return true;
}

} // namespace abcd
