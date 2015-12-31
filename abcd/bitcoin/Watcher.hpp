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
 * Provides threading support for the TxUpdater object.
 */
class Watcher:
    public TxCallbacks
{
public:
    Watcher(TxDatabase &db);

    // - Updater messages: -------------
    void disconnect();
    void connect();
    void watch_address(const bc::payment_address &address, unsigned poll_ms=10000);
    void prioritize_address(const bc::payment_address &address);
    void sendTx(DataSlice tx);

    // - Callbacks: --------------------
    typedef std::function<void (const bc::transaction_type &)> tx_callback;
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

    Watcher(const Watcher &copy) = delete;
    Watcher &operator=(const Watcher &copy) = delete;

private:
    zmq::context_t ctx_;
    TxUpdater txu_;

    bc::payment_address priority_address_;

    // Socket for talking to the thread:
    std::mutex socket_mutex_;
    std::string socket_name_;
    zmq::socket_t socket_;

    // Methods for sending messages on that socket:
    void send_disconnect();
    void send_connect();
    void send_watch_addr(bc::payment_address address, unsigned poll_ms);
    void sendSend(DataSlice tx);

    // The thread uses these callbacks, so put them in a mutex:
    std::mutex cb_mutex_;
    tx_callback cb_;
    block_height_callback height_cb_;
    quiet_callback quiet_cb_;

    // Everything below this point is only touched by the thread:
    bool command(uint8_t *data, size_t size);

    // TxCallbacks interface:
    virtual void on_add(const bc::transaction_type &tx) override;
    virtual void on_height(size_t height) override;
    virtual void on_quiet() override;
};

} // namespace abcd

#endif
