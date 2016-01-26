/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_TCP_CONNECTION_HPP
#define ABCD_BITCOIN_TCP_CONNECTION_HPP

#include "../util/Status.hpp"
#include "../util/Data.hpp"

namespace abcd {

class TcpConnection
{
public:
    ~TcpConnection();
    TcpConnection();

    /**
     * Connect to the specified server.
     */
    Status
    connect(const std::string &hostname, unsigned port);

    /**
     * Send some data over the socket.
     */
    Status
    send(DataSlice data);

    /**
     * Read all pending data from the socket (might not produce anything).
     */
    Status
    read(DataChunk &result);

    /**
     * Obtains a list of sockets that the main loop should sleep on.
     */
    int pollfd() const { return fd_; }

private:
    int fd_;
};

} // namespace abcd

#endif
