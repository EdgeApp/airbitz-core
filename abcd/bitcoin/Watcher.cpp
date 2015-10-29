/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Watcher.hpp"
#include "../General.hpp"
#include "../util/Debug.hpp"
#include <sstream>

using namespace libbitcoin;

namespace abcd {

using std::placeholders::_1;
using std::placeholders::_2;

constexpr unsigned default_poll = 10000;
constexpr unsigned priority_poll = 1000;

static unsigned watcher_id = 0;

// The last obelisk server we connected to:
static unsigned gLastObelisk = 0;

enum {
    msg_quit,
    msg_disconnect,
    msg_connect,
    msg_watch_addr,
    msg_send
};

Watcher::~Watcher()
{
}

Watcher::Watcher(TxDatabase &db):
    db_(db),
    socket_(ctx_, ZMQ_PAIR),
    connection_(nullptr)
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

static bool is_valid(const payment_address& address)
{
    return address.version() != payment_address::invalid_version;
}

void Watcher::connect()
{
    send_connect();
    for (auto& address: addresses_)
        send_watch_addr(address.first, address.second);
    if (is_valid(priority_address_))
        send_watch_addr(priority_address_, priority_poll);
}

void Watcher::send_tx(const transaction_type& tx)
{
    send_send(tx);
}

void Watcher::watch_address(const payment_address& address, unsigned poll_ms)
{
    auto a = addresses_.find(address);
    if (a != addresses_.end() && a->second == poll_ms)
        return;
    addresses_[address] = poll_ms;
    send_watch_addr(address, poll_ms);
}

/**
 * Checks a particular address more frequently (every other poll). To go back
 * to normal mode, pass an empty address.
 */
void Watcher::prioritize_address(const payment_address& address)
{
    if (is_valid(priority_address_))
    {
        send_watch_addr(priority_address_, default_poll);
        ABC_DebugLog("DISABLE prioritize_address %s", priority_address_.encoded().c_str());
    }
    priority_address_ = address;
    if (is_valid(priority_address_))
    {
        send_watch_addr(priority_address_, priority_poll);
        ABC_DebugLog("ENABLE prioritize_address %s", priority_address_.encoded().c_str());
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

void Watcher::send_watch_addr(payment_address address, unsigned poll_ms)
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

void Watcher::send_send(const transaction_type& tx)
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    std::basic_ostringstream<uint8_t> stream;
    auto serial = bc::make_serializer(std::ostreambuf_iterator<uint8_t>(stream));
    serial.write_byte(msg_send);
    serial.set_iterator(satoshi_save(tx, serial.iterator()));
    auto str = stream.str();
    socket_.send(str.data(), str.size());
}

bool Watcher::command(uint8_t* data, size_t size)
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
        doConnect().log();
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
            else
            {
                db_.insert(tx, TxState::unsent);
                on_add(tx);
            }
        }
        return true;
    }
}

Status
Watcher::doConnect()
{
    // Pick a server:
    auto servers = generalBitcoinServers();
    ++gLastObelisk;
    if (servers.size() <= gLastObelisk)
        gLastObelisk = 0;
    auto server = servers[gLastObelisk];

    // Parse out the key part:
    std::string key;
    size_t key_start = server.find(' ');
    if (key_start != std::string::npos)
    {
        key = server.substr(key_start + 1);
        server.erase(key_start);
    }

    ABC_DebugLog("Connecting to %s", server.c_str());
    delete connection_;
    connection_ = new connection(db_, ctx_, *this);
    if (!connection_->socket.connect(server, key))
    {
        delete connection_;
        connection_ = nullptr;
        connect();
        return ABC_ERROR(ABC_CC_SysError, "Cannot connect to " + server);
    }
    connection_->txu.start();

    return Status();
}

void Watcher::on_add(const transaction_type& tx)
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

void Watcher::on_send(const std::error_code& error, const transaction_type& tx)
{
}

void Watcher::on_quiet()
{
    std::lock_guard<std::mutex> lock(cb_mutex_);
    if (quiet_cb_)
        quiet_cb_();
}

void Watcher::on_fail()
{
    connect();
}

Watcher::connection::~connection()
{
}

static void on_unknown_nop(const std::string&)
{
}

Watcher::connection::connection(TxDatabase &db, void *ctx, TxCallbacks &cb)
  : socket(ctx),
    codec(socket, on_unknown_nop, std::chrono::seconds(10), 0),
    txu(db, codec, cb)
{
}

} // namespace abcd
