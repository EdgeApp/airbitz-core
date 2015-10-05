/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_BITCOIN_TX_DATABASE_HPP
#define ABCD_BITCOIN_TX_DATABASE_HPP

#include <bitcoin/bitcoin.hpp>
#include <mutex>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
#include <time.h>
#include <vector>
#include <algorithm>

namespace abcd {

enum class TxState
{
    /// The transaction has not been broadcast to the network.
    unsent,
    /// The network has seen this transaction, but not in a block.
    unconfirmed,
    /// The transaction is in a block.
    confirmed
};

typedef std::unordered_set<bc::payment_address> AddressSet;

/**
 * A single row in the transaction database.
 */
struct TxRow
{
    // The transaction itself:
    bc::transaction_type tx;
    bc::hash_digest tx_hash;
    bc::hash_digest tx_id;

    // State machine:
    TxState state;
    long long block_height;
    time_t timestamp;
    bool bMalleated;
    bool bMasterConfirm;
    //bc::hash_digest block_hash; // TODO: Fix obelisk to return this

    // The transaction is certainly in a block, but there is some
    // question whether or not that block is on the main chain:
    bool need_check;
};

/**
 * A list of transactions.
 *
 * This will eventually become a full database with queires mirroring what
 * is possible in the new libbitcoin-server protocol. For now, the goal is
 * to get something working.
 *
 * The fork-detection algorithm isn't perfect yet, since obelisk doesn't
 * provide the necessary information.
 */
class TxDatabase
{
public:
    ~TxDatabase();
    TxDatabase(unsigned unconfirmed_timeout=20*60);

    /**
     * Returns the highest block that this database has seen.
     */
    long long last_height();

    /**
     * Returns true if the database contains a transaction matching malleable tx_hash.
     */
    bool has_tx_hash(bc::hash_digest tx_hash);

    /**
     * Returns true if the database contains a transaction matching non-malleable tx_id.
     */
    bool has_tx_id(bc::hash_digest tx_id);

    /**
     * Obtains a transaction from the database using the malleable tx_hash.
     */
    bc::transaction_type get_tx_hash(bc::hash_digest tx_hash);

    /**
     * Obtains a transaction from the database using the non-malleable tx_id.
     */
    bc::transaction_type get_tx_id(bc::hash_digest tx_id);

    /**
     * Finds a transaction's height, or 0 if it isn't in a block.
     * Uses non-malleable tx_id
     */
    long long get_txid_height(bc::hash_digest tx_id);

    /**
     * Finds a transaction's height, or 0 if it isn't in a block.
     * Uses malleable tx hash
     */
    long long get_txhash_height(bc::hash_digest tx_hash);

    /**
     * Returns true if all inputs are addresses in the list control.
     */
    bool is_spend(bc::hash_digest tx_hash,
        const AddressSet &addresses);

    /**
     * Returns true if this address has received any funds.
     */
    bool has_history(const bc::payment_address &address);

    /**
     * Get all unspent outputs in the database.
     */
    bc::output_info_list get_utxos();

    /**
     * Get just the utxos corresponding to a set of addresses.
     */
    bc::output_info_list get_utxos(const AddressSet &addresses);

    /**
     * Write the database to an in-memory blob.
     */
    bc::data_chunk serialize();

    /**
     * Reconstitute the database from an in-memory blob.
     */
    bool load(const bc::data_chunk &data);

    /**
     * Debug dump to show db contents.
     */
    void dump(std::ostream &out);

    /**
     * Insert a new transaction into the database.
     * @return true if the callback should be fired.
     */
    bool insert(const bc::transaction_type &tx, TxState state);

    /*
     * Convert a transaction hash into a non-malleable tx_id hash
     */
    bc::hash_digest get_non_malleable_txid(bc::transaction_type tx);

private:
    // - Updater: ----------------------
    friend class TxUpdater;

    /**
     * Updates the block height.
     */
    void at_height(size_t height);

    /**
     * Mark a transaction as confirmed.
     * TODO: Require the block hash as well, once obelisk provides this.
     */
    void confirmed(bc::hash_digest tx_hash, long long block_height);

    /**
     * Mark a transaction as unconfirmed.
     */
    void unconfirmed(bc::hash_digest tx_hash);

    /**
     * Delete a transaction.
     * This can happen when the network rejects a spend request.
     */
    void forget(bc::hash_digest tx_hash);

    /**
     * Call this each time the server reports that it sees a transaction.
     */
    void reset_timestamp(bc::hash_digest tx_hash);

    typedef std::function<void (bc::hash_digest tx_hash)> HashFn;
    void foreach_unconfirmed(HashFn &&f);
    void foreach_forked(HashFn &&f);

    typedef std::function<void (const bc::transaction_type &tx)> TxFn;
    void foreach_unsent(TxFn &&f);

    // - Internal: ---------------------
    void check_fork(size_t height);

    // Guards access to object state:
    std::mutex mutex_;

    // The last block seen on the network:
    size_t last_height_;

    std::unordered_map<bc::hash_digest, TxRow> rows_;

    /*
     * Returns a vector of TxRow that match the unmalleable txid
     */
    std::vector<TxRow *> findByTxID(bc::hash_digest tx_id);

    // Number of seconds an unconfirmed transaction must remain unseen
    // before we stop saving it:
    const unsigned unconfirmed_timeout_;
};

} // namespace abcd

#endif
