/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "TcpConnection.hpp"
#include <errno.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

namespace abcd {

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

        if (0 == ::connect(fd_, p->ai_addr, p->ai_addrlen))
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
