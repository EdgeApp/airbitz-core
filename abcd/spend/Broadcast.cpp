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
#include <thread>

namespace abcd {

static Status
blockcypherPostTx(DataSlice tx)
{
    const char *url = isTestnet() ?
        "https://api.blockcypher.com/v1/btc/test3/txs/push":
        "https://api.blockcypher.com/v1/btc/main/txs/push";

    struct ChainJson: public JsonObject
    {
        ABC_JSON_STRING(tx, "tx", nullptr);
    } json;
    json.txSet(base16Encode(tx).c_str());
    std::string body;
    ABC_CHECK(json.encode(body));

    HttpReply reply;
    ABC_CHECK(HttpRequest().post(reply, url, body));
    ABC_CHECK(reply.codeOk());

    return Status();
}

static Status
chainPostTx(DataSlice tx)
{
    static const std::string auth =
        "Basic " + base64Encode(std::string(CHAIN_API_USERPWD));
    const char *url = isTestnet() ?
        "https://api.chain.com/v1/testnet3/transactions":
        "https://api.chain.com/v1/bitcoin/transactions";

    struct ChainJson: public JsonObject
    {
        ABC_JSON_STRING(hex, "hex", nullptr);
    } json;
    json.hexSet(base16Encode(tx).c_str());
    std::string body;
    ABC_CHECK(json.encode(body));

    HttpReply reply;
    ABC_CHECK(HttpRequest().header("Authorization", auth).
        put(reply, url, body));
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

template<Status (*f)(DataSlice tx)> void
shim(Status *result, DataSlice tx)
{
    *result = f(tx);
}

Status
broadcastTx(DataSlice rawTx)
{
    Status s1, s2, s3;
    auto t1 = std::thread(shim<chainPostTx>, &s1, rawTx);
    auto t2 = std::thread(shim<blockchainPostTx>, &s2, rawTx);
    auto t3 = std::thread(shim<blockcypherPostTx>, &s3, rawTx);

    t1.join();
    t2.join();
    t3.join();

    if (s1) return s1;
    if (s2) return s2;
    return s3;
}

} // namespace abcd
