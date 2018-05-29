/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_CACHE_TX_CACHE_HPP
#define ABCD_BITCOIN_CACHE_TX_CACHE_HPP

#include "../Typedefs.hpp"
#include <bitcoin/bitcoin.hpp>
#include <list>
#include <mutex>

namespace abcd {

class BlockCache;
class JsonObject;

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
 * An unspent transaction output.
 */
struct TxOutput
{
    libbitcoin::output_point point;
    uint64_t value;
    bool isSpendable; // Not RBF or double-spent.
    bool isIncoming; // Unconfirmed incoming funds.
};

typedef std::list<TxOutput> TxOutputList;

/**
 * Translates a list of `TxOutput` structures to the libbitcoin equivalent.
 * @param filter true to filter out unconfirmed outputs.
 */
libbitcoin::output_info_list
filterOutputs(const TxOutputList &utxos, bool filter=false);

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
    // Lifetime -----------------------------------------------------------

    TxCache(BlockCache &blockCache);

    /**
     * Clears the database for debugging purposes.
     */
    void
    clear();

    /**
     * Reads the database contents from the provided cache JSON object.
     */
    Status
    load(JsonObject &json);

    /**
     * Saves the database contents to the provided cache JSON object.
     */
    Status
    save(JsonObject &json);

    // Queries ------------------------------------------------------------

    /**
     * Obtains a transaction from the database.
     */
    Status
    get(bc::transaction_type &result, const std::string &txid) const;

    /**
     * Returns the input & output information for a loose transaction.
     */
    Status
    info(TxInfo &result, const bc::transaction_type &tx) const;

    /**
     * Looks up a transaction and returns its input & output information.
     */
    Status
    info(TxInfo &result, const std::string &txid) const;

    /**
     * Returns true if the transaction or its inputs
     * are missing from the cache.
     */
    bool
    missing(const std::string &txid) const;

    /**
     * Verifies that the given transactions are present in the cache
     * along with their inputs.
     * @return A list of needed txids.
     */
    TxidSet
    missingTxids(const TxidSet &txids) const;

    /**
     * Looks up a transaction and returns its confirmation & safety state.
     */
    Status
    status(TxStatus &result, const std::string &txid) const;

    /**
     * Lists all the transactions relevant to these addresses,
     * along with their information. Skips missing txids.
     */
    std::list<std::pair<TxInfo, TxStatus> >
    statuses(const TxidSet &txids) const;

    /**
     * Get just the utxos corresponding to a set of addresses.
     */
    TxOutputList
    utxos(const AddressSet &addresses) const;

    // Updates ------------------------------------------------------------

    /**
     * Removes a transaction from the cache if it is old and unconfirmed.
     * @return true if the transaction was removed.
     */
    bool
    drop(const std::string &txid, time_t now=time(nullptr));

    /**
     * Insert a new transaction into the database.
     * @return true if the callback should be fired.
     */
    bool
    insert(const bc::transaction_type &tx, std::string txid="");

    /**
     * Mark a transaction as confirmed.
     * TODO: Require the block hash as well, once obelisk provides this.
     */
    void
    confirmed(const std::string &txid, size_t height, time_t now=time(nullptr));

private:
    friend class TxGraph;

    struct HeightInfo
    {
        size_t height = 0;
        time_t firstSeen = 0;
    };

    mutable std::mutex mutex_;
    std::map<std::string, bc::transaction_type> txs_;
    std::map<std::string, HeightInfo> heights_;
    BlockCache &blocks_;

    /**
     * Same as `txInfo`, but should be called with the mutex held.
     */
    Status
    infoInternal(TxInfo &result, const bc::transaction_type &tx) const;

    /**
     * Returns true if the transaction has incoming non-change funds.
     */
    bool
    isIncoming(const bc::transaction_type &tx, const std::string &txid,
               const AddressSet &addresses) const;

    /**
     * Returns a transaction's height, or zero if it is unconfirmed.
     */
    size_t
    txidHeight(const std::string &txid) const;
};

} // namespace abcd

#endif
