/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */
#ifndef ABCD_BITCOIN_WATCHER_HPP
#define ABCD_BITCOIN_WATCHER_HPP

#include <bitcoin/watcher/tx_updater.hpp>
#include <bitcoin/client.hpp>
#include <zmq.hpp>
#include <iostream>
#include <unordered_map>

namespace abcd {

/**
 * Maintains a connection to an obelisk server, and uses that connection to
 * watch one or more bitcoin addresses for activity.
 */
class BC_API watcher
  : public libwallet::tx_callbacks
{
public:
    BC_API ~watcher();
    BC_API watcher();

    // - Server: -----------------------
    BC_API void disconnect();
    BC_API void connect(const std::string& server);

    // - Serialization: ----------------
    BC_API bc::data_chunk serialize();
    BC_API bool load(const bc::data_chunk& data);

    // - Addresses: --------------------
    BC_API void watch_address(const bc::payment_address& address, unsigned poll_ms=10000);
    BC_API void prioritize_address(const bc::payment_address& address);

    // - Transactions: -----------------
    BC_API void send_tx(const bc::transaction_type& tx);
    BC_API bc::transaction_type find_tx(bc::hash_digest txid);
    BC_API bool get_tx_height(bc::hash_digest txid, int& height);
    BC_API bc::output_info_list get_utxos(const bc::payment_address& address);
    BC_API bc::output_info_list get_utxos(bool filter=false);

    // - Chain height: -----------------
    BC_API size_t get_last_block_height();

    // - Callbacks: --------------------
    typedef std::function<void (const bc::transaction_type&)> callback;
    BC_API void set_callback(callback&& cb);

    typedef std::function<void (std::error_code, const bc::transaction_type&)> tx_sent_callback;
    BC_API void set_tx_sent_callback(tx_sent_callback&& cb);

    typedef std::function<void (const size_t)> block_height_callback;
    BC_API void set_height_callback(block_height_callback&& cb);

    typedef std::function<void ()> fail_callback;
    BC_API void set_fail_callback(fail_callback&& cb);

    // - Thread implementation: --------

    /**
     * Tells the loop() method to return.
     */
    BC_API void stop();

    /**
     * Call this function from a separate thread. It will run for an
     * unlimited amount of time as it works to keep the transactions
     * in the watcher up-to-date with the network. The function will
     * eventually return when the watcher object is destroyed.
     */
    BC_API void loop();

    watcher(const watcher& copy) = delete;
    watcher& operator=(const watcher& copy) = delete;

    // Debugging code:
    BC_API void dump(std::ostream& out=std::cout);

private:
    libwallet::tx_db db_;
    zmq::context_t ctx_;

    // Helper stuff:
    bool is_spend(const bc::transaction_type& tx);

    // Cached addresses, for when we are disconnected:
    std::unordered_map<bc::payment_address, unsigned> addresses_;
    bc::payment_address priority_address_;

    // Socket for talking to the thread:
    std::mutex socket_mutex_;
    std::string socket_name_;
    zmq::socket_t socket_;

    // Methods for sending messages on that socket:
    void send_disconnect();
    void send_connect(std::string server);
    void send_watch_addr(bc::payment_address address, unsigned poll_ms);
    void send_send(const bc::transaction_type& tx);

    // The thread uses these callbacks, so put them in a mutex:
    std::mutex cb_mutex_;
    callback cb_;
    block_height_callback height_cb_;
    tx_sent_callback tx_send_cb_;
    fail_callback fail_cb_;

    // Everything below this point is only touched by the thread:

    // Active connection (if any):
    struct connection
    {
        ~connection();
        connection(libwallet::tx_db& db, void *ctx, tx_callbacks& cb);

        bc::client::zeromq_socket socket;
        bc::client::obelisk_codec codec;
        libwallet::tx_updater txu;
    };
    connection* connection_;

    bool command(uint8_t* data, size_t size);

    // tx_callbacks interface:
    virtual void on_add(const bc::transaction_type& tx);
    virtual void on_height(size_t height);
    virtual void on_send(const std::error_code& error, const bc::transaction_type& tx);
    virtual void on_fail();
};

} // namespace abcd

#endif

