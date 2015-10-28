/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_WATCHER_HPP
#define ABCD_BITCOIN_WATCHER_HPP

#include "TxUpdater.hpp"
#include "../util/Status.hpp"
#include "../../minilibs/libbitcoin-client/client.hpp"
#include <zmq.hpp>
#include <iostream>
#include <unordered_map>

namespace abcd {

/**
 * Maintains a connection to an obelisk server, and uses that connection to
 * watch one or more bitcoin addresses for activity.
 */
class Watcher:
    public TxCallbacks
{
public:
    ~Watcher();
    Watcher(TxDatabase &db);

    // - Server: -----------------------
    void disconnect();
    void connect();

    // - Addresses: --------------------
    void watch_address(const bc::payment_address& address, unsigned poll_ms=10000);
    void prioritize_address(const bc::payment_address& address);

    // - Transactions: -----------------
    void send_tx(const bc::transaction_type& tx);
    bool ntxidHeight(bc::hash_digest txid, int &height);
    bc::output_info_list get_utxos(const bc::payment_address& address);
    bc::output_info_list get_utxos(bool filter=false);

    // - Callbacks: --------------------
    typedef std::function<void (const bc::transaction_type&)> tx_callback;
    void set_tx_callback(tx_callback cb);

    typedef std::function<void (const size_t)> block_height_callback;
    void set_height_callback(block_height_callback cb);

    typedef std::function<void ()> quiet_callback;
    void set_quiet_callback(quiet_callback cb);

    // - Thread implementation: --------

    /**
     * Tells the loop() method to return.
     */
    void stop();

    /**
     * Call this function from a separate thread. It will run for an
     * unlimited amount of time as it works to keep the transactions
     * in the watcher up-to-date with the network. The function will
     * eventually return when the watcher object is destroyed.
     */
    void loop();

    Watcher(const Watcher& copy) = delete;
    Watcher& operator=(const Watcher& copy) = delete;

    /**
     * Accesses the real database.
     * This should be const, but the db is not const-safe due to mutexes.
     */
    TxDatabase &db() { return db_; }

private:
    TxDatabase &db_;
    zmq::context_t ctx_;

    // Cached addresses, for when we are disconnected:
    std::unordered_map<bc::payment_address, unsigned> addresses_;
    bc::payment_address priority_address_;

    // Socket for talking to the thread:
    std::mutex socket_mutex_;
    std::string socket_name_;
    zmq::socket_t socket_;

    // Methods for sending messages on that socket:
    void send_disconnect();
    void send_connect();
    void send_watch_addr(bc::payment_address address, unsigned poll_ms);
    void send_send(const bc::transaction_type& tx);

    // The thread uses these callbacks, so put them in a mutex:
    std::mutex cb_mutex_;
    tx_callback cb_;
    block_height_callback height_cb_;
    quiet_callback quiet_cb_;

    // Everything below this point is only touched by the thread:

    // Active connection (if any):
    struct connection
    {
        ~connection();
        connection(TxDatabase &db, void *ctx, TxCallbacks &cb);

        bc::client::zeromq_socket socket;
        bc::client::obelisk_codec codec;
        TxUpdater txu;
    };
    connection* connection_;

    bool command(uint8_t* data, size_t size);
    Status doConnect();

    // TxCallbacks interface:
    virtual void on_add(const bc::transaction_type& tx) override;
    virtual void on_height(size_t height) override;
    virtual void on_send(const std::error_code& error, const bc::transaction_type& tx) override;
    virtual void on_quiet() override;
    virtual void on_fail() override;
};

} // namespace abcd

#endif
