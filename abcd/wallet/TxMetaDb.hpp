/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_WALLET_TX_META_DB_HPP
#define ABCD_WALLET_TX_META_DB_HPP

#include "../json/JsonPtr.hpp"
#include "../util/Status.hpp"
#include "TxMetadata.hpp"
#include <map>
#include <mutex>
#include <vector>

namespace abcd {

class Wallet;

struct Tx
{
    std::string ntxid;
    std::string txid;
    int64_t timeCreation;
    bool internal;
    TxMetadata metadata;
};

/**
 * Manages the transaction metadata stored in the wallet sync directory.
 */
class TxMetaDb
{
public:
    TxMetaDb(const Wallet &wallet);

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
    save(const Tx &tx);

    /**
     * Looks up a particular transaction in the database.
     */
    Status
    get(Tx &result, const std::string &ntxid);

private:
    mutable std::mutex mutex_;
    const Wallet &wallet_;
    const std::string dir_;

    std::map<std::string, Tx> txs_;
    std::map<std::string, JsonPtr> files_;

    std::string
    path(const Tx &tx);
};

} // namespace abcd

#endif
