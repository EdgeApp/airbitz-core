/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_BITCOIN_TX_UPDATER_HPP
#define ABCD_BITCOIN_TX_UPDATER_HPP

#include "TxDatabase.hpp"
#include <bitcoin/client.hpp>
#include <unordered_map>

namespace abcd {

/**
 * Interface containing the events the updater can trigger.
 */
class TxCallbacks
{
public:
    virtual ~TxCallbacks() {};

    /**
     * Called when the updater inserts a transaction into the database.
     */
    virtual void on_add(const bc::transaction_type &tx) = 0;

    /**
     * Called when the updater detects a new block.
     */
    virtual void on_height(size_t height) = 0;

    /**
     * Called when the updater has validated a transaction for send.
     */
    virtual void on_send(const std::error_code &error,
        const bc::transaction_type &tx) = 0;

    /**
     * Called when the updater has finished all its address queries,
     * and balances should now be up-to-date.
     */
    virtual void on_quiet() {}

    /**
     * Called when the updater sees an unexpected obelisk server failure.
     */
    virtual void on_fail() = 0;
};

/**
 * Syncs a set of transactions with the bitcoin server.
 */
class TxUpdater:
    public bc::client::sleeper
{
public:
    ~TxUpdater();
    TxUpdater(TxDatabase &db, bc::client::obelisk_codec &codec,
        TxCallbacks &callbacks);
    void start();

    void watch(const bc::payment_address &address,
        bc::client::sleep_time poll);
    void send(bc::transaction_type tx);

    AddressSet watching();

    // Sleeper interface:
    virtual bc::client::sleep_time wakeup();

private:
    void watch(bc::hash_digest tx_hash, bool want_inputs);
    void get_inputs(const bc::transaction_type &tx);
    void query_done();
    void queue_get_indices();

    // Server queries:
    void get_height();
    void get_tx(bc::hash_digest tx_hash, bool want_inputs);
    void get_tx_mem(bc::hash_digest tx_hash, bool want_inputs);
    void get_index(bc::hash_digest tx_hash);
    void send_tx(const bc::transaction_type &tx);
    void query_address(const bc::payment_address &address);

    TxDatabase &db_;
    bc::client::obelisk_codec &codec_;
    TxCallbacks &callbacks_;

    struct AddressRow
    {
        libbitcoin::client::sleep_time poll_time;
        std::chrono::steady_clock::time_point last_check;
    };
    std::unordered_map<bc::payment_address, AddressRow> rows_;

    bool failed_;
    size_t queued_queries_;
    size_t queued_get_indices_;
    std::chrono::steady_clock::time_point last_wakeup_;
};

} // namespace abcd

#endif
