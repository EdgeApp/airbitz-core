/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */
#include "watcher.hpp"

#include <sstream>

using namespace libbitcoin;
using namespace libwallet;

namespace abcd {

using std::placeholders::_1;
using std::placeholders::_2;

constexpr unsigned default_poll = 10000;
constexpr unsigned priority_poll = 1000;

static unsigned watcher_id = 0;

enum {
    msg_quit,
    msg_disconnect,
    msg_connect,
    msg_watch_tx,
    msg_watch_addr,
    msg_send
};

BC_API watcher::~watcher()
{
}

BC_API watcher::watcher()
  : socket_(ctx_, ZMQ_PAIR),
    connection_(nullptr)
{
    std::stringstream name;
    name << "inproc://watcher-" << watcher_id++;
    socket_name_ = name.str();
    socket_.bind(socket_name_.c_str());
    int linger = 0;
    socket_.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
}

BC_API void watcher::disconnect()
{
    send_disconnect();
}

static bool is_valid(const payment_address& address)
{
    return address.version() != payment_address::invalid_version;
}

BC_API void watcher::connect(const std::string& server)
{
    send_connect(server);
    for (auto& address: addresses_)
        send_watch_addr(address.first, address.second);
    if (is_valid(priority_address_))
        send_watch_addr(priority_address_, priority_poll);
}

BC_API void watcher::send_tx(const transaction_type& tx)
{
    send_send(tx);
}

/**
 * Serializes the database for storage while the app is off.
 */
BC_API data_chunk watcher::serialize()
{
    return db_.serialize();
}

BC_API bool watcher::load(const data_chunk& data)
{
    return db_.load(data);
}

BC_API void watcher::watch_address(const payment_address& address, unsigned poll_ms)
{
    addresses_[address] = poll_ms;
    send_watch_addr(address, poll_ms);
}

/**
 * Checks a particular address more frequently (every other poll). To go back
 * to normal mode, pass an empty address.
 */
BC_API void watcher::prioritize_address(const payment_address& address)
{
    if (is_valid(priority_address_))
        send_watch_addr(priority_address_, default_poll);
    priority_address_ = address;
    if (is_valid(priority_address_))
        send_watch_addr(priority_address_, priority_poll);
}

BC_API transaction_type watcher::find_tx(hash_digest txid)
{
    return db_.get_tx(txid);
}

/**
 * Sets up the new-transaction callback. This callback will be called from
 * some random thread, so be sure to handle that with a mutex or such.
 */
BC_API void watcher::set_tx_callback(tx_callback cb)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    cb_ = std::move(cb);
}

/**
 * Sets up the change in block heightcallback.
 */
BC_API void watcher::set_height_callback(block_height_callback cb)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    height_cb_ = std::move(cb);
}

/**
 * Sets up the tx sent callback
 */
BC_API void watcher::set_tx_sent_callback(tx_sent_callback cb)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    tx_send_cb_ = std::move(cb);
}

/**
 * Sets up the server failure callback
 */
BC_API void watcher::set_quiet_callback(quiet_callback cb)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    quiet_cb_ = std::move(cb);
}

/**
 * Sets up the server failure callback
 */
BC_API void watcher::set_fail_callback(fail_callback cb)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    fail_cb_ = std::move(cb);
}

/**
 * Obtains a list of unspent outputs for an address. This is needed to spend
 * funds.
 */
BC_API output_info_list watcher::get_utxos(const payment_address& address)
{
    auto utxos = db_.get_utxos();
    output_info_list out;

    for (auto& utxo: utxos)
    {
        const auto& tx = db_.get_tx(utxo.point.hash);
        auto& output = tx.outputs[utxo.point.index];

        bc::payment_address to_address;
        if (bc::extract(to_address, output.script))
            if (address == to_address)
                out.push_back(utxo);
    }

    return out;
}

/**
 * Returns all the unspent transaction outputs in the wallet.
 * @param filter true to filter out unconfirmed outputs.
 */
BC_API output_info_list watcher::get_utxos(bool filter)
{
    auto utxos = db_.get_utxos();
    output_info_list out;

    for (auto& utxo: utxos)
    {
        const auto& tx = db_.get_tx(utxo.point.hash);
        auto& output = tx.outputs[utxo.point.index];

        bc::payment_address to_address;
        if (bc::extract(to_address, output.script))
            if (addresses_.find(to_address) != addresses_.end())
                if (!filter || db_.get_tx_height(utxo.point.hash) || is_spend(tx))
                    out.push_back(utxo);
    }

    return out;
}

BC_API size_t watcher::get_last_block_height()
{
    return db_.last_height();
}

BC_API bool watcher::get_tx_height(hash_digest txid, int& height)
{
    height = db_.get_tx_height(txid);
    return db_.has_tx(txid);
}

/**
 * Returns true if all inputs are addresses we control.
 */
bool watcher::is_spend(const bc::transaction_type& tx)
{
    for (auto& input: tx.inputs)
    {
        bc::payment_address address;
        if (!bc::extract(address, input.script))
            return false;
        if (addresses_.find(address) == addresses_.end())
            return false;
    }
    return true;
}

void watcher::dump(std::ostream& out)
{
    db_.dump(out);
}

BC_API void watcher::stop()
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    uint8_t req = msg_quit;
    socket_.send(&req, 1);
}

void throw_term()
{
    throw 1;
}
void throw_fault()
{
    throw 2;
}
void throw_intr()
{
    throw 3;
}

BC_API void watcher::loop()
{
    zmq::socket_t socket(ctx_, ZMQ_PAIR);
    socket.connect(socket_name_.c_str());
    int linger = 0;
    socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    bool done = false;
    while (!done)
    {
        int delay = -1;
        std::vector<zmq_pollitem_t> items;
        items.reserve(2);
        zmq_pollitem_t inproc_item = { socket, 0, ZMQ_POLLIN, 0 };
        items.push_back(inproc_item);
        if (connection_)
        {
            items.push_back(connection_->socket.pollitem());
            auto next_wakeup = connection_->codec.wakeup();
            next_wakeup = bc::client::min_sleep(next_wakeup,
                connection_->txu.wakeup());
            if (next_wakeup.count())
                delay = next_wakeup.count();
        }
        if (zmq_poll(items.data(), items.size(), delay) < 0)
            switch (errno)
            {
            case ETERM:  throw_term();  break;
            case EFAULT: throw_fault(); break;
            case EINTR:  throw_intr();  break;
            }

        if (connection_ && items[1].revents)
            connection_->socket.forward(connection_->codec);
        if (items[0].revents)
        {
            zmq::message_t msg;
            socket.recv(&msg);
            if (!command(static_cast<uint8_t*>(msg.data()), msg.size()))
                done = true;
        }
    }
    delete connection_;
}

void watcher::send_disconnect()
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    uint8_t req = msg_disconnect;
    socket_.send(&req, 1);
}

void watcher::send_connect(std::string server)
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    std::basic_ostringstream<uint8_t> stream;
    auto serial = bc::make_serializer(std::ostreambuf_iterator<uint8_t>(stream));
    serial.write_byte(msg_connect);
    serial.write_data(server);
    auto str = stream.str();
    socket_.send(str.data(), str.size());
}

void watcher::send_watch_addr(payment_address address, unsigned poll_ms)
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    std::basic_ostringstream<uint8_t> stream;
    auto serial = bc::make_serializer(std::ostreambuf_iterator<uint8_t>(stream));
    serial.write_byte(msg_watch_addr);
    serial.write_byte(address.version());
    serial.write_short_hash(address.hash());
    serial.write_4_bytes(poll_ms);
    auto str = stream.str();
    socket_.send(str.data(), str.size());
}

void watcher::send_send(const transaction_type& tx)
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    std::basic_ostringstream<uint8_t> stream;
    auto serial = bc::make_serializer(std::ostreambuf_iterator<uint8_t>(stream));
    serial.write_byte(msg_send);
    serial.set_iterator(satoshi_save(tx, serial.iterator()));
    auto str = stream.str();
    socket_.send(str.data(), str.size());
}

bool watcher::command(uint8_t* data, size_t size)
{
    auto serial = bc::make_deserializer(data, data + size);
    switch (serial.read_byte())
    {
    default:
    case msg_quit:
        delete connection_;
        connection_ = nullptr;
        return false;

    case msg_disconnect:
        delete connection_;
        connection_ = nullptr;
        return true;

    case msg_connect:
        {
            std::string server(data + 1, data + size);
            delete connection_;
            connection_ = new connection(db_, ctx_, *this);
            if (!connection_->socket.connect(server))
            {
                delete connection_;
                connection_ = nullptr;
                return true;
            }
            connection_->txu.start();
        }
        return true;

    case msg_watch_addr:
        {
            auto version = serial.read_byte();
            auto hash = serial.read_short_hash();
            payment_address address(version, hash);
            bc::client::sleep_time poll_time(serial.read_4_bytes());
            if (connection_)
                connection_->txu.watch(address, poll_time);
        }
        return true;

    case msg_send:
        {
            transaction_type tx;
            bc::satoshi_load(serial.iterator(), data + size, tx);
            if (connection_)
                connection_->txu.send(tx);
        }
        return true;
    }
}

void watcher::on_add(const transaction_type& tx)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (cb_)
        cb_(tx);
}

void watcher::on_height(size_t height)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (height_cb_)
        height_cb_(height);
}

void watcher::on_send(const std::error_code& error, const transaction_type& tx)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (tx_send_cb_)
        tx_send_cb_(error, tx);
}

void watcher::on_quiet()
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (quiet_cb_)
        quiet_cb_();
}

void watcher::on_fail()
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (fail_cb_)
        fail_cb_();
}

watcher::connection::~connection()
{
}

watcher::connection::connection(tx_db& db, void *ctx, tx_callbacks& cb)
  : socket(ctx),
    codec(socket),
    txu(db, codec, cb)
{
}

} // abcd

