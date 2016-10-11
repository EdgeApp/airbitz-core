/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Watcher.hpp"
#include "../util/Debug.hpp"
#include <bitcoin/bitcoin.hpp>
#include <sstream>

namespace abcd {

zmq::context_t &zmqContext()
{
    static std::mutex mutex;
    static std::unique_ptr<zmq::context_t> ctx;

    std::lock_guard<std::mutex> lock(mutex);
    if (!ctx)
        ctx.reset(new zmq::context_t);

    return *ctx;
}

using std::placeholders::_1;
using std::placeholders::_2;

static unsigned watcher_id = 0;

enum
{
    msg_quit,
    msg_wakeup,
    msg_disconnect,
    msg_connect,
    msg_send
};

Watcher::Watcher(Cache &cache):
    socket_(zmqContext(), ZMQ_PAIR),
    txu_(cache, zmqContext())
{
    std::stringstream name;
    name << "inproc://watcher-" << watcher_id++;
    socket_name_ = name.str();
    socket_.bind(socket_name_.c_str());
    int linger = 0;
    socket_.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));
}

void
Watcher::sendWakeup()
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    uint8_t req = msg_wakeup;
    socket_.send(&req, 1);
}

void Watcher::disconnect()
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    uint8_t req = msg_disconnect;
    socket_.send(&req, 1);
}

void Watcher::connect()
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    uint8_t req = msg_connect;
    socket_.send(&req, 1);
}

void
Watcher::sendTx(StatusCallback status, DataSlice tx)
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    auto statusCopy = new StatusCallback(std::move(status));
    auto statusInt = reinterpret_cast<uintptr_t>(statusCopy);

    auto data = buildData({bc::to_byte(msg_send),
                           bc::to_little_endian(statusInt), tx
                          });
    socket_.send(data.data(), data.size());
}

void Watcher::stop()
{
    std::lock_guard<std::mutex> lock(socket_mutex_);

    uint8_t req = msg_quit;
    socket_.send(&req, 1);

    // Log time to start logout
    ABC_DebugLog("Watcher::stop() %lu", this);

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
    zmq::socket_t socket(zmqContext(), ZMQ_PAIR);
    socket.connect(socket_name_.c_str());
    int linger = 0;
    socket.setsockopt(ZMQ_LINGER, &linger, sizeof(linger));

    bool done = false;
    while (!done)
    {
        auto nextWakeup = txu_.wakeup();
        int delay = nextWakeup.count() ? nextWakeup.count() : -1;

        std::vector<zmq_pollitem_t> items;
        items.push_back(zmq_pollitem_t{ socket, 0, ZMQ_POLLIN, 0 });
        auto txuItems = txu_.pollitems();
        items.insert(items.end(), txuItems.begin(), txuItems.end());

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
}

bool Watcher::command(uint8_t *data, size_t size)
{
    auto serial = bc::make_deserializer(data, data + size);
    switch (serial.read_byte())
    {
    default:
    case msg_quit:
        // Log time to finish watcher.
        ABC_DebugLog("Watcher Successfully Quit %lu", this);
        return false;

    case msg_wakeup:
        return true;

    case msg_disconnect:
        txu_.disconnect();
        return true;

    case msg_connect:
        txu_.connect().log();
        return true;

    case msg_send:
    {
        auto statusInt = serial.read_little_endian<uintptr_t>();
        auto statusCopy = reinterpret_cast<StatusCallback *>(statusInt);
        txu_.sendTx(*statusCopy, DataSlice(serial.iterator(), data + size));
        delete statusCopy;
    }
    return true;
    }
}

} // namespace abcd
