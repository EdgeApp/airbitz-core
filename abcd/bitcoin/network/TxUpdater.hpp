/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_NETWORK_TX_UPDATER_HPP
#define ABCD_BITCOIN_NETWORK_TX_UPDATER_HPP

#include "StratumConnection.hpp"
#include "../../../minilibs/libbitcoin-client/client.hpp"

namespace abcd {

class Cache;

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
     * Called when the updater has finished all its address queries,
     * and balances should now be up-to-date.
     */
    virtual void on_quiet() {}
};

/**
 * Syncs a set of transactions with the bitcoin server.
 */
class TxUpdater:
    public bc::client::sleeper
{
public:
    ~TxUpdater();
    TxUpdater(Cache &cache, void *ctx, TxCallbacks &callbacks);

    void disconnect();
    Status connect();
    void send(StatusCallback status, DataSlice tx);

    // Sleeper interface:
    virtual bc::client::sleep_time wakeup();

    /**
     * Obtains a list of sockets that the main loop should sleep on.
     */
    std::vector<zmq_pollitem_t>
    pollitems();

private:
    enum class ConnectionType
    {
        libbitcoin,
        stratum
    };

    struct Connection
    {
        Connection(void *ctx, long server_index);

        bool operator==(const Connection &l)
        {
            return server_index == l.server_index;
        };

        ConnectionType type;
        StratumConnection stratumCodec;
        bc::client::zeromq_socket bc_socket;
        bc::client::obelisk_codec bc_codec;
        int queued_queries_;
        int queued_get_indices_;
        int queued_get_height_;

        long server_index;
    };

    Status connectTo(long index);

    void watch_tx(bc::hash_digest txid, bool want_inputs, int idx, size_t index);
    void get_inputs(const bc::transaction_type &tx, int idx);
    void query_done(int idx, Connection &bconn);

    // Server queries:
    void get_height();
    void get_tx(bc::hash_digest txid, bool want_inputs, int idx);
    void get_tx_mem(bc::hash_digest txid, bool want_inputs, int idx);
    void get_index(bc::hash_digest txid, int idx);
    void sendTx(StatusCallback status, DataSlice tx);
    void query_address(const bc::payment_address &address, int server_index);

    Cache &cache_;
    void *ctx_;
    TxCallbacks &callbacks_;

    bool failed_;
    long failed_server_idx_;
    std::chrono::steady_clock::time_point last_wakeup_;

    bool wantConnection = false;
    std::vector<Connection *> connections_;
    std::vector<std::string> serverList_;
    std::set<int> untriedLibbitcoin_;
    std::set<int> untriedStratum_;
};

} // namespace abcd

#endif
