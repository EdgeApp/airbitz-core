/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Watcher.hpp"
#include "../util/Debug.hpp"
#include <sstream>

namespace abcd {

using std::placeholders::_1;
using std::placeholders::_2;

constexpr unsigned default_poll = 20000;
constexpr unsigned priority_poll = 4000;

static unsigned watcher_id = 0;

enum
{
    msg_quit,
    msg_disconnect,
    msg_connect,
    msg_watch_addr,
    msg_send
};

static bool is_valid(const bc::payment_address &address)
{
    return address.version() != bc::payment_address::invalid_version;
}

Watcher::Watcher(TxDatabase &db):
    txu_(db, ctx_, *this),
    socket_(ctx_, ZMQ_PAIR)
{
    std::stringstream name;
    name << "inproc://watcher-" << watcher_id++;
    socket_name_ = name.str();
    socket_.bind(socket_name_.c_str());
    int linger = 0;
    socket_.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
}

void Watcher::disconnect()
{
    send_disconnect();
}

void Watcher::connect()
{
    send_connect();
}

void Watcher::send_tx(const bc::transaction_type &tx)
{
    send_send(tx);
}

void
Watcher::watch_address(const bc::payment_address &address, unsigned poll_ms)
{
    send_watch_addr(address, poll_ms);
}

/**
 * Checks a particular address more frequently.
 * To go back to normal mode, pass an empty address.
 */
void Watcher::prioritize_address(const bc::payment_address &address)
{
    if (is_valid(priority_address_))
    {
        send_watch_addr(priority_address_, default_poll);
        ABC_DebugLog("DISABLE prioritize_address %s",
                     priority_address_.encoded().c_str());
    }
    priority_address_ = address;
    if (is_valid(priority_address_))
    {
        send_watch_addr(priority_address_, priority_poll);
        ABC_DebugLog("ENABLE prioritize_address %s",
                     priority_address_.encoded().c_str());
    }
}

/**
 * Sets up the new-transaction callback. This callback will be called from
 * some random thread, so be sure to handle that with a mutex or such.
 */
void Watcher::set_tx_callback(tx_callback cb)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    cb_ = std::move(cb);
}

/**
 * Sets up the change in block heightcallback.
 */
void Watcher::set_height_callback(block_height_callback cb)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    height_cb_ = std::move(cb);
}

/**
 * Sets up the server failure callback
 */
void Watcher::set_quiet_callback(quiet_callback cb)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    quiet_cb_ = std::move(cb);
}

void Watcher::stop()
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

void Watcher::loop()
{
    zmq::socket_t socket(ctx_, ZMQ_PAIR);
    socket.connect(socket_name_.c_str());
    int linger = 0;
    socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    txu_.connect().log();

    bool done = false;
    while (!done)
    {
        std::vector<zmq_pollitem_t> items;
        items.push_back(zmq_pollitem_t{ socket, 0, ZMQ_POLLIN, 0 });
        auto txuItems = txu_.pollitems();
        items.insert(items.end(), txuItems.begin(), txuItems.end());

        auto nextWakeup = txu_.wakeup();
        int delay = nextWakeup.count() ? nextWakeup.count() : -1;

        if (zmq_poll(items.data(), items.size(), delay) < 0)
            switch (errno)
            {
            case ETERM:
                throw_term();
                break;
            case EFAULT:
                throw_fault();
                break;
            case EINTR:
                throw_intr();
                break;
            }

        if (items[0].revents)
        {
            zmq::message_t msg;
            socket.recv(&msg);
            if (!command(static_cast<uint8_t *>(msg.data()), msg.size()))
                done = true;
        }
    }
    txu_.disconnect();
}

void Watcher::send_disconnect()
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    uint8_t req = msg_disconnect;
    socket_.send(&req, 1);
}

void Watcher::send_connect()
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    uint8_t req = msg_connect;
    socket_.send(&req, 1);
}

void Watcher::send_watch_addr(bc::payment_address address, unsigned poll_ms)
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

void Watcher::send_send(const bc::transaction_type &tx)
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    std::basic_ostringstream<uint8_t> stream;
    auto serial = bc::make_serializer(std::ostreambuf_iterator<uint8_t>(stream));
    serial.write_byte(msg_send);
    serial.set_iterator(satoshi_save(tx, serial.iterator()));
    auto str = stream.str();
    socket_.send(str.data(), str.size());
}

bool Watcher::command(uint8_t *data, size_t size)
{
    auto serial = bc::make_deserializer(data, data + size);
    switch (serial.read_byte())
    {
    default:
    case msg_quit:
        txu_.disconnect();
        return false;

    case msg_disconnect:
        txu_.disconnect();
        return true;

    case msg_connect:
        txu_.connect().log();
        return true;

    case msg_watch_addr:
    {
        auto version = serial.read_byte();
        auto hash = serial.read_short_hash();
        bc::payment_address address(version, hash);
        bc::client::sleep_time poll_time(serial.read_4_bytes());
        txu_.watch(address, poll_time);
    }
    return true;

    case msg_send:
    {
        bc::transaction_type tx;
        bc::satoshi_load(serial.iterator(), data + size, tx);
        txu_.send(tx);
    }
    return true;
    }
}

void Watcher::on_add(const bc::transaction_type &tx)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (cb_)
        cb_(tx);
}

void Watcher::on_height(size_t height)
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (height_cb_)
        height_cb_(height);
}

void Watcher::on_quiet()
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (quiet_cb_)
        quiet_cb_();
}

} // namespace abcd
