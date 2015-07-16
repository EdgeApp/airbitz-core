/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */

#include "TxUpdater.hpp"
#include "../util/Debug.hpp"

namespace abcd {

using std::placeholders::_1;

TxUpdater::~TxUpdater()
{
}

TxUpdater::TxUpdater(TxDatabase &db, bc::client::obelisk_codec &codec,
    TxCallbacks &callbacks):
    db_(db), codec_(codec),
    callbacks_(callbacks),
    failed_(false),
    queued_queries_(0),
    queued_get_indices_(0),
    last_wakeup_(std::chrono::steady_clock::now())
{
}

void TxUpdater::start()
{
    // Check for new blocks:
    get_height();

    // Handle block-fork checks & unconfirmed transactions:
    db_.foreach_unconfirmed(std::bind(&TxUpdater::get_index, this, _1));
    queue_get_indices();

    // Transmit all unsent transactions:
    db_.foreach_unsent(std::bind(&TxUpdater::send_tx, this, _1));
}

void TxUpdater::watch(const bc::payment_address &address,
    bc::client::sleep_time poll)
{
    // Only insert if it isn't already present:
    rows_[address] = AddressRow{poll, std::chrono::steady_clock::now()};
    query_address(address);
}

void TxUpdater::send(bc::transaction_type tx)
{
    if (db_.insert(tx, TxState::unsent))
        callbacks_.on_add(tx);
    send_tx(tx);
}

AddressSet TxUpdater::watching()
{
    AddressSet out;
    for (auto &row: rows_)
        out.insert(row.first);
    return out;
}

bc::client::sleep_time TxUpdater::wakeup()
{
    bc::client::sleep_time next_wakeup(0);
    auto now = std::chrono::steady_clock::now();

    // Figure out when our next block check is:
    auto period = std::chrono::seconds(30);
    auto elapsed = std::chrono::duration_cast<bc::client::sleep_time>(
        now - last_wakeup_);
    if (period <= elapsed)
    {
        get_height();
        last_wakeup_ = now;
        elapsed = bc::client::sleep_time::zero();
    }
    next_wakeup = period - elapsed;

    // Figure out when our next address check should be:
    for (auto &row: rows_)
    {
        auto poll_time = row.second.poll_time;
        auto elapsed = std::chrono::duration_cast<bc::client::sleep_time>(
            now - row.second.last_check);
        if (poll_time <= elapsed)
        {
            if (queued_queries_ < 10 ||
                row.second.poll_time < std::chrono::seconds(2))
            {
                row.second.last_check = now;
                next_wakeup = bc::client::min_sleep(next_wakeup, poll_time);
                query_address(row.first);
            }
        }
        else
            next_wakeup = bc::client::min_sleep(next_wakeup, poll_time - elapsed);
    }

    // Report the last server failure:
    if (failed_)
    {
        callbacks_.on_fail();
        failed_ = false;
    }

    return next_wakeup;
}

void TxUpdater::watch(bc::hash_digest tx_hash, bool want_inputs)
{
    db_.reset_timestamp(tx_hash);
    if (!db_.has_tx(tx_hash))
        get_tx(tx_hash, want_inputs);
    else if (want_inputs)
        get_inputs(db_.get_tx(tx_hash));
}

void TxUpdater::get_inputs(const bc::transaction_type &tx)
{
    for (auto &input: tx.inputs)
        watch(input.previous_output.hash, false);
}

void TxUpdater::query_done()
{
    --queued_queries_;
    if (!queued_queries_)
        callbacks_.on_quiet();
}

void TxUpdater::queue_get_indices()
{
    if (queued_get_indices_)
        return;
    db_.foreach_forked(std::bind(&TxUpdater::get_index, this, _1));
}

// - server queries --------------------

void TxUpdater::get_height()
{
    auto on_error = [this](const std::error_code &error)
    {
        if (!failed_)
            ABC_DebugLog("fetch_last_height failed: %s",
                error.message().c_str());
        failed_ = true;
    };

    auto on_done = [this](size_t height)
    {
        if (height != db_.last_height())
        {
            db_.at_height(height);
            callbacks_.on_height(height);

            // Query all unconfirmed transactions:
            db_.foreach_unconfirmed(std::bind(&TxUpdater::get_index, this, _1));
            queue_get_indices();
        }
    };

    codec_.fetch_last_height(on_error, on_done);
}

void TxUpdater::get_tx(bc::hash_digest tx_hash, bool want_inputs)
{
    ++queued_queries_;

    auto on_error = [this, tx_hash, want_inputs](const std::error_code &error)
    {
        // A failure means the transaction might be in the mempool:
        (void)error;
        get_tx_mem(tx_hash, want_inputs);
        query_done();
    };

    auto on_done = [this, tx_hash, want_inputs](const bc::transaction_type &tx)
    {
        BITCOIN_ASSERT(tx_hash == bc::hash_transaction(tx));
        if (db_.insert(tx, TxState::unconfirmed))
            callbacks_.on_add(tx);
        if (want_inputs)
            get_inputs(tx);
        get_index(tx_hash);
        query_done();
    };

    codec_.fetch_transaction(on_error, on_done, tx_hash);
}

void TxUpdater::get_tx_mem(bc::hash_digest tx_hash, bool want_inputs)
{
    ++queued_queries_;

    auto on_error = [this](const std::error_code &error)
    {
        if (!failed_)
            ABC_DebugLog("fetch_unconfirmed_transaction failed: %s",
                error.message().c_str());
        failed_ = true;
        query_done();
    };

    auto on_done = [this, tx_hash, want_inputs](const bc::transaction_type &tx)
    {
        BITCOIN_ASSERT(tx_hash == bc::hash_transaction(tx));
        if (db_.insert(tx, TxState::unconfirmed))
            callbacks_.on_add(tx);
        if (want_inputs)
            get_inputs(tx);
        get_index(tx_hash);
        query_done();
    };

    codec_.fetch_unconfirmed_transaction(on_error, on_done, tx_hash);
}

void TxUpdater::get_index(bc::hash_digest tx_hash)
{
    ++queued_get_indices_;

    auto on_error = [this, tx_hash](const std::error_code &error)
    {
        // A failure means that the transaction is unconfirmed:
        (void)error;
        db_.unconfirmed(tx_hash);

        --queued_get_indices_;
        queue_get_indices();
    };

    auto on_done = [this, tx_hash](size_t block_height, size_t index)
    {
        // The transaction is confirmed:
        (void)index;

        db_.confirmed(tx_hash, block_height);

        --queued_get_indices_;
        queue_get_indices();
    };

    codec_.fetch_transaction_index(on_error, on_done, tx_hash);
}

void TxUpdater::send_tx(const bc::transaction_type &tx)
{
    auto on_error = [this, tx](const std::error_code &error)
    {
        //server_fail(error);
        db_.forget(bc::hash_transaction(tx));
        callbacks_.on_send(error, tx);
    };

    auto on_done = [this, tx]()
    {
        std::error_code error;
        db_.unconfirmed(bc::hash_transaction(tx));
        callbacks_.on_send(error, tx);
    };

    codec_.broadcast_transaction(on_error, on_done, tx);
}

void TxUpdater::query_address(const bc::payment_address &address)
{
    ++queued_queries_;

    auto on_error = [this](const std::error_code &error)
    {
        if (!failed_)
            ABC_DebugLog("address_fetch_history failed: %s",
                error.message().c_str());
        failed_ = true;
        query_done();
    };

    auto on_done = [this](const bc::blockchain::history_list &history)
    {
        for (auto &row: history)
        {
            watch(row.output.hash, true);
            if (row.spend.hash != bc::null_hash)
                watch(row.spend.hash, true);
        }
        query_done();
    };

    codec_.address_fetch_history(on_error, on_done, address);
}

} // namespace abcd
