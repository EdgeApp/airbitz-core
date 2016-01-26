/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/bitcoin/StratumConnection.hpp"
#include <iostream>

using namespace abcd;

static int done = 0;

COMMAND(InitLevel::context, CliStratumVersion, "stratum-version",
        " <server>")
{
    if (1 != argc)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto uri = argv[0];

    // Connect to the server:
    StratumConnection c;
    ABC_CHECK(c.connect(uri));
    std::cout << "Connection established" << std::endl;

    // Send the version command:
    auto onError = [](Status status)
    {
        std::cout << "Got error " << status << std::endl;
        ++done;
    };
    auto onReply = [](const std::string &version)
    {
        std::cout << "Version: " << version << std::endl;
        ++done;
        return Status();
    };
    c.version(onError, onReply);

    // Main loop:
    while (true)
    {
        SleepTime sleep;
        ABC_CHECK(c.wakeup(sleep));
        if (1 <= done)
            break;

        zmq_pollitem_t pollitem =
        {
            nullptr, c.pollfd(), ZMQ_POLLIN, ZMQ_POLLOUT
        };
        long timeout = sleep.count() ? sleep.count() : -1;
        zmq_poll(&pollitem, 1, timeout);
    }

    return Status();
}
