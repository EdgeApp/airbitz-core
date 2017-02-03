/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/bitcoin/Text.hpp"
#include "../../abcd/bitcoin/WatcherBridge.hpp"
#include "../../abcd/bitcoin/cache/Cache.hpp"
#include "../../abcd/wallet/Wallet.hpp"
#include <unistd.h>
#include <signal.h>
#include <iostream>
#include <thread>

using namespace abcd;

static bool running = true;

static void
signalCallback(int dummy)
{
    running = false;
}

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

static void showReport(Wallet &wallet)
{
    time_t sleepTime;
    const auto statuses = wallet.cache.addresses.statuses(sleepTime);
    const auto progress = wallet.cache.addresses.progress();

    std::cout << "Finished " << progress.first << " out of " <<
              progress.second << " addresses." << std::endl;
    for (const auto &status: statuses)
    {
        const auto missing = status.missingTxids.size();

        // Basic address information:
        std::cout << "Address " << status.address << " incomplete.";
        if (status.dirty)
            std::cout << " Dirty.";
        if (status.needsCheck)
            std::cout << " Unsubscribed.";
        if (missing)
            std::cout << " Missing " << missing << " txids:";
        std::cout << std::endl;

        // Txid list:
        if (missing < 10)
        {
            for (const auto &txid: status.missingTxids)
                std::cout << "   " << txid << std::endl;
        }
        else
        {
            std::cout << "  <list too long>" << std::endl;
        }
    }
}

/**
 * Launches and runs a watcher thread.
 */
class WatcherThread
{
public:
    ~WatcherThread();

    abcd::Status
    init(const Session &session);

private:
    std::string uuid_;
    std::thread *thread_ = nullptr;
};

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

COMMAND(InitLevel::wallet, Watcher, "watcher",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    WatcherThread thread;
    ABC_CHECK(thread.init(session));

    // The command stops with ctrl-c:
    signal(SIGINT, signalCallback);
    while (running)
    {
        showReport(*session.wallet);
        sleep(5);
    }

    return Status();
}

COMMAND(InitLevel::wallet, WatcherUpdate, "watcher-update",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    WatcherThread thread;
    ABC_CHECK(thread.init(session));

    // The command stops with ctrl-c, or when progress is 100%:
    signal(SIGINT, signalCallback);
    while (running)
    {
        showReport(*session.wallet);
        const auto progress = session.wallet->cache.addresses.progress();
        if (progress.first == progress.second)
            break;
        sleep(5);
    }

    return Status();
}

COMMAND(InitLevel::wallet, WatcherSweep, "watcher-sweep",
        " <wif>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto wif = argv[0];

    WatcherThread thread;
    ABC_CHECK(thread.init(session));

    // Begin the sweep:
    ParsedUri parsed;
    ABC_CHECK(parseUri(parsed, wif));
    if (parsed.address.empty())
        return ABC_ERROR(ABC_CC_ParseError, "Cannot parse address");
    ABC_CHECK(bridgeSweepKey(*session.wallet, parsed.wif, parsed.address));

    // The command stops with ctrl-c:
    signal(SIGINT, signalCallback);
    while (running)
        sleep(1);

    return Status();
}
