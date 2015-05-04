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

namespace abcd {

struct ChainJson:
    public JsonObject
{
    ABC_JSON_STRING(hex, "hex", nullptr);
};

static Status
chainPostTx(DataSlice tx)
{
    static const std::string auth =
        "Basic " + base64Encode(std::string(CHAIN_API_USERPWD));
    const char *url = isTestnet() ?
        "https://api.chain.com/v1/testnet3/transactions":
        "https://api.chain.com/v1/bitcoin/transactions";

    ChainJson object;
    object.hexSet(base16Encode(tx).c_str());
    std::string body;
    ABC_CHECK(object.encode(body));

    HttpReply reply;
    ABC_CHECK(HttpRequest().header("Authorization", auth).
        put(reply, url, body));

    return Status();
}

static Status
blockhainPostTx(DataSlice tx)
{
    std::string body = "tx=" + base16Encode(tx);

    HttpReply reply;
    ABC_CHECK(HttpRequest().
        post(reply, "https://blockchain.info/pushtx", body));

    return Status();
}

Status
broadcastTx(DataSlice rawTx)
{
    Status out = chainPostTx(rawTx);

    // Only try Blockchain when not on testnet:
    if (!isTestnet())
    {
        if (out)
            blockhainPostTx(rawTx);
        else
            out = blockhainPostTx(rawTx);
    }

    return out;
}

} // namespace abcd
