/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_TX_DATABASE_HPP
#define ABCD_BITCOIN_TX_DATABASE_HPP

#include "Typedefs.hpp"
#include <bitcoin/bitcoin.hpp>
#include <time.h>
#include <list>
#include <mutex>
#include <ostream>
#include <unordered_map>

namespace abcd {

enum class TxState
{
    /// The network has seen this transaction, but not in a block.
    unconfirmed,
    /// The transaction is in a block.
    confirmed
};

/**
 * An input or an output of a transaction.
 */
struct TxInOut
{
    bool input;
    uint64_t value;
    std::string address;
};

/**
 * Transaction input & output information.
 */
struct TxInfo
{
    std::string txid;
    std::string ntxid;
    int64_t fee;
    std::list<TxInOut> ios;
};

/**
 * Transaction confirmation & safety status.
 */
struct TxStatus
{
    size_t height;
    bool isDoubleSpent;
    bool isReplaceByFee;
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
class TxCache
{
public:
    ~TxCache();
    TxCache(unsigned unconfirmed_timeout=60*60);

    /**
     * Returns the highest block that this database has seen.
     */
    long long last_height() const;

    /**
     * Obtains a transaction from the database.
     */
    Status
    txidLookup(bc::transaction_type &result, bc::hash_digest txid) const;

    /**
     * Finds a transaction's height, or 0 if it is unconfirmed.
     */
    long long txidHeight(bc::hash_digest txid) const;

    /**
     * Returns true if the transaction touches one of the addresses.
     */
    bool
    isRelevant(const bc::transaction_type &tx,
               const AddressSet &addresses) const;

    /**
     * Returns the input & output information for a loose transaction.
     */
    TxInfo
    txInfo(const bc::transaction_type &tx) const;

    /**
     * Looks up a transaction and returns its input & output information.
     */
    Status
    txidInfo(TxInfo &result, const std::string &txid) const;

    /**
     * Looks up a transaction and returns its confirmation & safety state.
     */
    Status
    txidStatus(TxStatus &result, const std::string &txid) const;

    /**
     * Lists all the transactions relevant to these addresses,
     * along with their information.
     */
    std::list<std::pair<TxInfo, TxStatus> >
    list(const AddressSet &addresses) const;

    /**
     * Returns true if this address has received any funds.
     */
    bool has_history(const bc::payment_address &address) const;

    /**
     * Get just the utxos corresponding to a set of addresses.
     * @param filter true to filter out unconfirmed outputs.
     */
    bc::output_info_list get_utxos(const AddressSet &addresses,
                                   bool filter=false) const;

    /**
     * Write the database to an in-memory blob.
     */
    bc::data_chunk serialize() const;

    /**
     * Reconstitute the database from an in-memory blob.
     */
    Status
    load(const bc::data_chunk &data);

    /**
     * Debug dump to show db contents.
     */
    void dump(std::ostream &out) const;

    /**
     * Insert a new transaction into the database.
     * @return true if the callback should be fired.
     */
    bool insert(const bc::transaction_type &tx);

    /**
     * Clears the database for debugging purposes.
     */
    void clear();

private:
    friend class TxUpdater;
    friend class TxGraph;
    friend class TxCacheTest;

    /**
     * A single row in the transaction database.
     */
    struct TxRow
    {
        // The transaction itself:
        bc::transaction_type tx;
        bc::hash_digest txid;
        bc::hash_digest ntxid;

        // State machine:
        TxState state;
        long long block_height;
        time_t timestamp;
        //bc::hash_digest block_hash; // TODO: Fix obelisk to return this
    };

    /**
     * Updates the block height.
     */
    void at_height(size_t height);

    /**
     * Mark a transaction as confirmed.
     * TODO: Require the block hash as well, once obelisk provides this.
     */
    void confirmed(bc::hash_digest txid, long long block_height);

    /**
     * Mark a transaction as unconfirmed.
     */
    void unconfirmed(bc::hash_digest txid);

    /**
     * Call this each time the server reports that it sees a transaction.
     */
    void reset_timestamp(bc::hash_digest txid);

    typedef std::function<void (bc::hash_digest txid)> HashFn;
    void foreach_unconfirmed(HashFn &&f);

    // - Internal: ---------------------

    /**
     * Returns true if the transaction touches one of the addresses.
     */
    bool
    isRelevantInternal(const bc::transaction_type &tx,
                       const AddressSet &addresses) const;

    /**
     * Same as `txInfo`, but should be called with the mutex held.
     */
    TxInfo
    txInfoInternal(const bc::transaction_type &tx) const;

    /**
     * Returns true if the transaction has incoming non-change funds.
     */
    bool
    isIncoming(const TxRow &row,
               const AddressSet &addresses) const;

    // Guards access to object state:
    mutable std::mutex mutex_;

    // The last block seen on the network:
    size_t last_height_;

    std::unordered_map<bc::hash_digest, TxRow> rows_;

    // Number of seconds an unconfirmed transaction must remain unseen
    // before we stop saving it:
    const unsigned unconfirmed_timeout_;
};

} // namespace abcd

#endif
