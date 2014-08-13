/**
 * @file
 * Reactor loop core implmentation for the ABC event subsystem.
 *
 * See LICENSE for copy, modification, and use permissions
 *
 * @author See AUTHORS
 */

#include "bouncer.hpp"

namespace abc {

#define CONTROL_ADDRESS "ipc://abc-bouncer"

// Cross-thread message ID's:
enum {
    BOUNCER_SHUTDOWN,
    BOUNCER_TIMEOUT,
    BOUNCER_ADD,
    BOUNCER_REMOVE
};

bouncer_thread::bouncer_thread(void *ctx)
  : ctx_(ctx), socket_(ctx),
    shutdown_(false), timeout_(0)
{
    socket_.connect(CONTROL_ADDRESS); // TODO: Check for errors!
}

bool bouncer_thread::wait()
{
    // Figure out timeout:
    long poll_time = -1;
    if (timeout_.count())
    {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - timeout_start_);
        if (timeout_ < elapsed)
            timeout_ = timeout_.zero();
        else
            poll_time = (timeout_ - elapsed).count();
    }

    // Figure out sockets:
    items_.clear();
    items_.reserve(2*bouncers_.size() + 1);
    for (auto i: bouncers_)
    {
        items_.push_back(i->local_socket_.pollitem());
        items_.push_back(i->remote_socket_.pollitem());
    }
    items_.push_back(socket_.pollitem());
    zmq_poll(items_.data(), items_.size(), poll_time);

    // Forward messages:
    for (size_t i = 0; i < items_.size(); ++i)
    {
        if (items_[i].revents)
        {
            if (i == items_.size() - 1)
                socket_.forward(*this);
            else if (i & 1)
                bouncers_[i/2]->remote_socket_.forward(
                    bouncers_[i/2]->local_socket_);
            else
                bouncers_[i/2]->local_socket_.forward(
                    bouncers_[i/2]->remote_socket_);
        }
    }
    return shutdown_;
}

/**
 * Handles messages sent to the bouncer itself.
 */
void bouncer_thread::message(const data_chunk& data, bool more)
{
    auto deserial = make_deserializer(data.begin(), data.end());
    int id = deserial.read_byte();

    switch (id) {
    case BOUNCER_SHUTDOWN:
        shutdown_ = true;
        break;

    case BOUNCER_TIMEOUT:
        timeout_ = std::chrono::milliseconds(deserial.read_4_bytes());
        timeout_start_ =  std::chrono::steady_clock::now();
        break;

    case BOUNCER_ADD:
        {
            std::string local = deserial.read_string();
            std::string remote = deserial.read_string();
            add(local, remote);
        }
        break;

    case BOUNCER_REMOVE:
        {
            std::string local = deserial.read_string();
            remove(local);
        }
        break;
    }
}

void bouncer_thread::add(std::string local, std::string remote)
{
    bouncers_.push_back(new bouncer(ctx_, local, remote));
}

void bouncer_thread::remove(std::string local)
{
    auto i = std::find_if(bouncers_.begin(), bouncers_.end(),
        [&local](bouncer* b){ return b->local_ == local; });
    if (i != bouncers_.end())
    {
        delete *i;
        bouncers_.erase(i);
    }
}

bouncer_thread::bouncer::bouncer(void *ctx, std::string local, std::string remote)
  : local_(local), local_socket_(ctx), remote_socket_(ctx)
{
    local_socket_.bind(local); // TODO: Check for errors!
    remote_socket_.connect(remote); // TODO: Check for errors!
}

bouncer_client::bouncer_client(void *ctx)
  : socket_(ctx)
{
    socket_.bind(CONTROL_ADDRESS); // TODO: Check for errors!
}

void bouncer_client::shutdown()
{
    data_chunk data;
    data.resize(1);
    auto serial = make_serializer(data.begin());
    serial.write_byte(BOUNCER_SHUTDOWN);
    BITCOIN_ASSERT(serial.iterator() == data.end());
    socket_.message(data, false);
}

void bouncer_client::set_timeout(std::chrono::milliseconds delay)
{
    data_chunk data;
    data.resize(1 + 4);
    auto serial = make_serializer(data.begin());
    serial.write_byte(BOUNCER_TIMEOUT);
    serial.write_4_bytes(delay.count());
    BITCOIN_ASSERT(serial.iterator() == data.end());
    socket_.message(data, false);
}

void bouncer_client::add_bouncer(std::string local, std::string remote)
{
    data_chunk data;
    data.resize(1 +
        variable_uint_size(local.size()) + local.size() +
        variable_uint_size(remote.size()) + remote.size());
    auto serial = make_serializer(data.begin());
    serial.write_byte(BOUNCER_ADD);
    serial.write_string(local);
    serial.write_string(remote);
    BITCOIN_ASSERT(serial.iterator() == data.end());
    socket_.message(data, false);
}

void bouncer_client::remove_bouncer(std::string local)
{
    data_chunk data;
    data.resize(1 +
        variable_uint_size(local.size()) + local.size());
    auto serial = make_serializer(data.begin());
    serial.write_byte(BOUNCER_REMOVE);
    serial.write_string(local);
    BITCOIN_ASSERT(serial.iterator() == data.end());
    socket_.message(data, false);
}

} // namespace abc
