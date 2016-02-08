/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../Util.hpp"
#include <unistd.h>
#include <signal.h>
#include <iostream>

using namespace abcd;

static bool running = true;

static void
signalCallback(int dummy)
{
    running = false;
}

COMMAND(InitLevel::wallet, Watcher, "watcher",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    bool dirty;
    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username.c_str(),
                                     session.password.c_str(),
                                     session.uuid.c_str(),
                                     &dirty, &error));
    {
        WatcherThread thread;
        ABC_CHECK(thread.init(session));

        // The command stops with ctrl-c:
        signal(SIGINT, signalCallback);
        while (running)
            sleep(1);
    }
    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username.c_str(),
                                     session.password.c_str(),
                                     session.uuid.c_str(),
                                     &dirty, &error));

    return Status();
}
