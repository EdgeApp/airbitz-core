/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Broadcast.hpp"
#include "Testnet.hpp"
#include "../config.h"
#include "../crypto/Encoding.hpp"
#include "../http/HttpRequest.hpp"
#include "../json/JsonObject.hpp"
#include <future>

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
    ABC_DebugLog(reply.body.c_str());
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
        ABC_ERROR(ABC_CC_Error, "No blockchain.info testnet");

    HttpReply reply;
    ABC_CHECK(HttpRequest().
        post(reply, "https://blockchain.info/pushtx", body));
    ABC_CHECK(reply.codeOk());

    return Status();
}

Status
broadcastTx(DataSlice rawTx)
{
    auto f1 = std::async(std::launch::async, chainPostTx, rawTx);
    auto f2 = std::async(std::launch::async, blockchainPostTx, rawTx);
    auto f3 = std::async(std::launch::async, blockcypherPostTx, rawTx);

    Status s1 = f1.get();
    Status s2 = f2.get();
    Status s3 = f3.get();

    if (s1) return s1;
    if (s2) return s2;
    return s3;
}

} // namespace abcd
