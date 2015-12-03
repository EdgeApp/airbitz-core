/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Broadcast.hpp"
#include "../bitcoin/Testnet.hpp"
#include "../config.h"
#include "../crypto/Encoding.hpp"
#include "../http/HttpRequest.hpp"
#include "../json/JsonObject.hpp"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace abcd {

static Status
blockcypherPostTx(DataSlice tx)
{
    const char *url = isTestnet() ?
                      "https://api.blockcypher.com/v1/btc/test3/txs/push":
                      "https://api.blockcypher.com/v1/btc/main/txs/push";

    struct BlockcypherJson:
        public JsonObject
    {
        ABC_JSON_STRING(tx, "tx", nullptr);
    } json;
    json.txSet(base16Encode(tx));

    HttpReply reply;
    ABC_CHECK(HttpRequest().post(reply, url, json.encode()));
    ABC_CHECK(reply.codeOk());

    return Status();
}

static Status
chainPostTx(DataSlice tx)
{
    static const std::string auth =
        "Basic " + base64Encode(std::string(CHAIN_API_USERPWD));
    const char *url = isTestnet() ?
                      "https://api.chain.com/v2/testnet3/transactions/send":
                      "https://api.chain.com/v2/bitcoin/transactions/send";

    struct ChainJson:
        public JsonObject
    {
        ABC_JSON_STRING(hex, "signed_hex", nullptr);
    } json;
    json.hexSet(base16Encode(tx));

    HttpReply reply;
    ABC_CHECK(HttpRequest().header("Authorization", auth).
              post(reply, url, json.encode()));
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
              post(reply, "https://blockchain.info/pushtx", body));
    ABC_CHECK(reply.codeOk());

    return Status();
}

/**
 * Handles a broadcast in the background.
 * The BroadcastThread signals the condition variable when it is done.
 * The use of `std::shared_ptr` allows the thread to continue holding
 * resources even after the calling function has finished.
 */
class BroadcastThread
{
public:
    BroadcastThread(std::shared_ptr<std::condition_variable> cv):
        cv_(cv)
    {}

    Status status()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }

    bool done()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return done_;
    }

    template<Status (*f)(DataSlice tx)>
    static void run(std::shared_ptr<BroadcastThread> self, DataChunk tx)
    {
        auto status = f(tx);
        std::lock_guard<std::mutex> lock(self->mutex_);
        self->status_ = status;
        self->done_ = true;
        self->cv_->notify_all();
    }

private:
    std::shared_ptr<std::condition_variable> cv_;
    std::mutex mutex_;
    Status status_;
    bool done_ = false;
};

Status
broadcastTx(DataSlice rawTx)
{
    // Create communication resources:
    std::mutex mutex;
    std::unique_lock<std::mutex> lock(mutex);
    auto cv = std::make_shared<std::condition_variable>();
    auto t1 = std::make_shared<BroadcastThread>(cv);
    auto t2 = std::make_shared<BroadcastThread>(cv);
    auto t3 = std::make_shared<BroadcastThread>(cv);

    // Launch the broadcasts:
    DataChunk tx(rawTx.begin(), rawTx.end());
    std::thread(BroadcastThread::run<chainPostTx>, t1, tx).detach();
    std::thread(BroadcastThread::run<blockchainPostTx>, t2, tx).detach();
    std::thread(BroadcastThread::run<blockcypherPostTx>, t3, tx).detach();

    // Loop as long as any thread is still running:
    while (!t1->done() || !t2->done() || !t3->done())
    {
        cv->wait(lock);
        // Quit immediately if one has succeeded:
        if ((t1->done() && t1->status()) ||
                (t2->done() && t2->status()) ||
                (t3->done() && t3->status()))
            return Status();
    }

    // We only get here if all three have failed:
    return t3->status();
}

} // namespace abcd
