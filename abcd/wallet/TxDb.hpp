/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_WALLET_TX_DB_HPP
#define ABCD_WALLET_TX_DB_HPP

#include "../json/JsonPtr.hpp"
#include "../util/Status.hpp"
#include "Metadata.hpp"
#include <map>
#include <mutex>
#include <vector>

namespace abcd {

class Wallet;

struct TxMeta
{
    std::string ntxid;
    std::string txid;
    time_t timeCreation;
    bool internal;
    uint64_t airbitzFeeWanted = 0; // The fee owed for this transaction
    int64_t airbitzFeeSent = 0; // The fee actually sent
    Metadata metadata;
};

/**
 * Manages the transaction metadata stored in the wallet sync directory.
 */
class TxDb
{
public:
    TxDb(const Wallet &wallet);

    /**
     * Loads the transactions off disk.
     */
    Status
    load();

    /**
     * Updates a particular transaction in the database.
     * Can also be used to insert new transactions into the database.
     */
    Status
    save(const TxMeta &tx, int64_t balance, int64_t fee);

    /**
     * Looks up a particular transaction in the database.
     */
    Status
    get(TxMeta &result, const std::string &ntxid);

    /**
     * Determine how many satoshis of unpaid Airbitz fees are in the wallet.
     */
    int64_t
    airbitzFeePending();

    /**
     * Determine the last time the wallet sent out Airbitz fees.
     */
    time_t
    airbitzFeeLastSent();

private:
    mutable std::mutex mutex_;
    const Wallet &wallet_;
    const std::string dir_;

    std::map<std::string, TxMeta> txs_;
    std::map<std::string, JsonPtr> files_;

    std::string
    path(const TxMeta &tx);
};

} // namespace abcd

#endif
