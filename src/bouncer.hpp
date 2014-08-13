/**
 * @file
 * Reactor loop core implmentation for the ABC event subsystem.
 *
 * See LICENSE for copy, modification, and use permissions
 *
 * @author See AUTHORS
 */

#ifndef ABC_BOUNCER_HPP
#define ABC_BOUNCER_HPP

#include <vector>
#include <bitcoin/client/zeromq_socket.hpp>

namespace abc {

using namespace libbitcoin;

/**
 * Contains the thread-side elements of the event-notification mechanism.
 * This object must be constructed and broken down within the context of
 * the bouncer thread.
 */
class bouncer_thread:
    private bc::client::message_stream
{
public:
    bouncer_thread(void *ctx);

    /**
     * Sleeps until the core would like to be woken up.
     * @return false indicates that the thread should shut down.
     */
    bool wait();

private:
    void message(const data_chunk& data, bool more);
    void add(std::string local, std::string remote);
    void remove(std::string local);

    // ZeroMQ stuff:
    void *ctx_;
    libbitcoin::client::zeromq_socket socket_;

    // Connections to monitor:
    struct bouncer
    {
        bouncer(void *ctx, std::string local, std::string remote);

        std::string local_;
        libbitcoin::client::zeromq_socket local_socket_;
        libbitcoin::client::zeromq_socket remote_socket_;
    };
    std::vector<bouncer *> bouncers_;
    std::vector<zmq_pollitem_t> items_;

    // Lifetime:
    bool shutdown_;
    std::chrono::milliseconds timeout_;
    std::chrono::steady_clock::time_point timeout_start_;
};

/**
 * Client-side element of the event-notification mechanism. This sends
 * control messages to the bouncer thread element.
 */
class bouncer_client
{
public:
    bouncer_client(void *ctx);

    void shutdown();
    void set_timeout(std::chrono::milliseconds delay);
    void add_bouncer(std::string local, std::string remote);
    void remove_bouncer(std::string local);

private:
    libbitcoin::client::zeromq_socket socket_;
};

} // namespace abc

#endif
