/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Broadcast.hpp"
#include "../bitcoin/Testnet.hpp"
#include "../bitcoin/WatcherBridge.hpp"
#include "../Context.hpp"
#include "../crypto/Encoding.hpp"
#include "../http/HttpRequest.hpp"
#include "../json/JsonObject.hpp"
#include "../util/Debug.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace abcd {

static Status
insightPostTx(DataSlice tx)
{
    std::string body = "rawtx=" + base16Encode(tx);

    const char *url = isTestnet() ?
                      "https://test-insight.bitpay.com/api/tx/send":
                      "https://insight.bitpay.com/api/tx/send";

    HttpReply reply;
    ABC_CHECK(HttpRequest().
              post(reply, url, body));
    ABC_CHECK(reply.codeOk());

    return Status();
}

static Status
blockchainPostTx(DataSlice tx)
{
    std::string body = "tx=" + base16Encode(tx);
    if (isTestnet())
        return ABC_ERROR(ABC_CC_Error, "No blockchain.info testnet");

    HttpReply reply;
    ABC_CHECK(HttpRequest().
              header("Content-Type", "application/x-www-form-urlencoded").
              post(reply, "https://blockchain.info/pushtx", body));
    ABC_CHECK(reply.codeOk());

    return Status();
}

/**
 * Contains a condition variable and its associated mutex.
 * The mutex protects access to some piece of shared data,
 * and the condition variable triggers a wakeup when the data is modified.
 */
struct Syncer
{
    std::condition_variable cv;
    std::mutex mutex;
};

/**
 * Holds the return value from a long-running background task,
 * and a flag indicating if the task is done yet.
 */
struct DelayedStatus
{
    bool done = false;
    Status status;
};

/**
 * A long-running broadcast task.
 * @param condition Guards access to the status,
 * and signals when the task is done.
 * @param status Holds the status of the task.
 */
template<Status (*f)(DataSlice tx)> static void
broadcastTask(std::shared_ptr<Syncer> syncer,
              std::shared_ptr<DelayedStatus> status, DataChunk tx)
{
    auto result = f(tx);

    {
        std::lock_guard<std::mutex> lock(syncer->mutex);
        status->status = result;
        status->done = true;
    }
    syncer->cv.notify_all();
}

Status
broadcastTx(Wallet &self, DataSlice rawTx)
{
    // Create communication resources:
    auto syncer = std::make_shared<Syncer>();
    auto s1 = std::make_shared<DelayedStatus>();
    auto s2 = std::make_shared<DelayedStatus>();
    auto s3 = std::make_shared<DelayedStatus>();

    // Launch the broadcasts:
    DataChunk tx(rawTx.begin(), rawTx.end());
    std::thread(broadcastTask<blockchainPostTx>, syncer, s1, tx).detach();
    std::thread(broadcastTask<insightPostTx>, syncer, s2, tx).detach();

    // Queue up an async broadcast over the TxUpdater:
    auto updaterDone = [syncer, s3](Status s)
    {
        {
            std::lock_guard<std::mutex> lock(syncer->mutex);
            s3->status = s;
            s3->done = true;
            if (s)
                ABC_DebugLog("Stratum broadcast OK");
            else
                s.log();
        }
        syncer->cv.notify_all();
    };
    watcherSend(self, updaterDone, rawTx).log();

    // Loop as long as any thread is still running:
    while (true)
    {
        // Wait for the condition variable, which also acquires the lock:
        std::unique_lock<std::mutex> lock(syncer->mutex);
        syncer->cv.wait(lock);

        // Stop waiting if any broadcast has succeeded:
        if (s1->done && s1->status)
            break;
        if (s2->done && s2->status)
            break;
        if (s3->done && s3->status)
            break;

        // If they are all done, we have an error:
        if (s1->done && s2->done && s3->done)
            return s1->status;
    }

    return Status();
}

} // namespace abcd
