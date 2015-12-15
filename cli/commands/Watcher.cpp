/*
 * Copyright (c) 2015, AirBitz, Inc.
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
syncCallback(const tABC_AsyncBitCoinInfo *pInfo)
{
}

static void
signalCallback(int dummy)
{
    running = false;
}

COMMAND(InitLevel::wallet, Watcher, "watcher")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... watcher <user> <pass> <wallet>");

    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username.c_str(),
                                     session.password.c_str(),
                                     session.uuid.c_str(),
                                     syncCallback, nullptr, &error));
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
                                     syncCallback, nullptr, &error));

    return Status();
}
