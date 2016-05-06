/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Util.hpp"
#include "Command.hpp"
#include <iostream>

using namespace abcd;

static void
eventCallback(const tABC_AsyncBitCoinInfo *pInfo)
{
    switch (pInfo->eventType)
    {
    case ABC_AsyncEventType_IncomingBitCoin:
        std::cout << "Incoming transaction" << std::endl;
        break;
    case ABC_AsyncEventType_BlockHeightChange:
        std::cout << "Block height change" << std::endl;
        break;
    default:
        break;
    }
}

static void
watcherThread(const char *uuid)
{
    tABC_Error error;
    ABC_WatcherLoop(uuid, eventCallback, nullptr, &error);
}

WatcherThread::~WatcherThread()
{
    if (thread_)
    {
        tABC_Error error;
        ABC_WatcherStop(uuid_.c_str(), &error);
        thread_->join();
        ABC_WatcherDelete(uuid_.c_str(), &error);
        delete thread_;
    }
}

Status
WatcherThread::init(const Session &session)
{
    uuid_ = session.uuid;
    ABC_CHECK_OLD(ABC_WatcherStart(session.username.c_str(),
                                   session.password.c_str(),
                                   session.uuid.c_str(),
                                   &error));
    thread_ = new std::thread(watcherThread, session.uuid.c_str());
    ABC_CHECK_OLD(ABC_WatcherConnect(session.uuid.c_str(), &error));
    return Status();
}
