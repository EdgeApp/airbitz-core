/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "TcpConnection.hpp"
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace abcd {

static int
timeoutConnect(int sock, struct sockaddr *addr,
                socklen_t addr_len, struct timeval *tv)
{
    fd_set fdset;
    int flags = 0;
    int so_error;
    socklen_t len = sizeof(so_error);

    if ((flags = fcntl(sock, F_GETFL, 0)) < 0)
        return -1;

    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) < 0)
        return -1;

    if (connect(sock, addr, addr_len) == 0)
        goto exit;

    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);

    if (select(sock + 1, NULL, &fdset, NULL, tv) > 0)
    {
        getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
        if (so_error == 0)
            goto exit;
    }
    return -1;

exit:
    if (fcntl(sock, F_SETFL, flags) < 0)
        return -1;
    return 0;
}

TcpConnection::~TcpConnection()
{
    if (0 < fd_)
        close(fd_);
}

TcpConnection::TcpConnection():
    fd_(0)
{
}

Status
TcpConnection::connect(const std::string &hostname, unsigned port)
{
    // Do the DNS lookup:
    struct addrinfo hints {};
    struct addrinfo *list = nullptr;
    hints.ai_family = AF_UNSPEC; // Allow IPv6 or IPv4
    hints.ai_socktype = SOCK_STREAM; // TCP only
    if (getaddrinfo(hostname.c_str(), std::to_string(port).c_str(), &hints, &list))
        return ABC_ERROR(ABC_CC_ServerError, "Cannot look up " + hostname);

    // Try the returned DNS entries until one connects:
    for (struct addrinfo *p = list; p; ++p)
    {
        fd_ = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd_ < 0)
            return ABC_ERROR(ABC_CC_ServerError, "Cannot create socket");

        struct timeval sto;
        sto.tv_sec = 10;
        sto.tv_usec = 0;
        if (0 == timeoutConnect(fd_, p->ai_addr, p->ai_addrlen, &sto))
            break;

        close(fd_);
        fd_ = 0;
    }
    if (fd_ < 0)
        return ABC_ERROR(ABC_CC_ServerError, "Cannot connect to " + hostname);

    return Status();
}

Status
TcpConnection::send(DataSlice data)
{
    while (data.size())
    {
        auto bytes = ::send(fd_, data.data(), data.size(), 0);
        if (bytes < 0)
            return ABC_ERROR(ABC_CC_ServerError, "Failed to send");

        data = DataSlice(data.data() + bytes, data.end());
    }

    return Status();
}

Status
TcpConnection::read(DataChunk &result)
{
    unsigned char data[1024];
    auto bytes = recv(fd_, data, sizeof(data), MSG_DONTWAIT);
    if (bytes < 0)
    {
        if (EAGAIN != errno && EWOULDBLOCK != errno)
            return ABC_ERROR(ABC_CC_ServerError, "Cannot read from socket");

        // No data, but that's fine:
        bytes = 0;
    }

    result = DataChunk(data, data + bytes);
    return Status();
}

} // namespace abcd
