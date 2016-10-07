/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_WATCHER_HPP
#define ABCD_BITCOIN_WATCHER_HPP

#include "network/TxUpdater.hpp"
#include <zmq.hpp>
#include <mutex>

namespace abcd {

/**
 * Provides threading support for the TxUpdater object.
 */
class Watcher
{
public:
    Watcher(Cache &cache);

    // - Updater messages: -------------
    void sendWakeup();
    void disconnect();
    void connect();
    void sendTx(StatusCallback status, DataSlice tx);

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
    // Socket for talking to the thread:
    std::mutex socket_mutex_;
    std::string socket_name_;
    zmq::socket_t socket_;

    // Everything below this point is only touched by the thread:
    bool command(uint8_t *data, size_t size);

    // This needs to be constructed last, since it uses everything else:
    TxUpdater txu_;
};

} // namespace abcd

#endif
