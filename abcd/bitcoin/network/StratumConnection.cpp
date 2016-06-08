/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "StratumConnection.hpp"
#include "../Utility.hpp"
#include "../../crypto/Encoding.hpp"
#include "../../http/Uri.hpp"
#include "../../json/JsonArray.hpp"
#include "../../json/JsonObject.hpp"
#include "../../util/Debug.hpp"
#include <algorithm>

namespace abcd {

constexpr std::chrono::seconds keepaliveTime(60);
constexpr std::chrono::seconds timeout(10);

struct RequestJson:
    public JsonObject
{
    ABC_JSON_INTEGER(id, "id", 0)
    ABC_JSON_STRING(method, "method", nullptr)
    ABC_JSON_VALUE(params, "params", JsonArray);
};

struct HeaderJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(HeaderJson, JsonObject)

    ABC_JSON_INTEGER(nonce, "nonce", 0);
    ABC_JSON_STRING(prev_block_hash, "prev_block_hash", "");
    ABC_JSON_INTEGER(timestamp, "timestamp", 0);
    ABC_JSON_STRING(merkle_root, "merkle_root", "");
    ABC_JSON_INTEGER(block_height, "block_height", 0);
    ABC_JSON_INTEGER(version, "version", 0);
    ABC_JSON_INTEGER(bits, "bits", 0);
};

struct ReplyJson:
    public JsonObject
{
    ABC_JSON_INTEGER(id, "id", 0)
    ABC_JSON_VALUE(result, "result", JsonPtr);

    // Only used on subscription updates:
    ABC_JSON_STRING(method, "method", "");
    ABC_JSON_VALUE(params, "params", JsonPtr);
};

StratumConnection::~StratumConnection()
{
    for (auto &i: pending_)
        i.second.onError(ABC_ERROR(ABC_CC_Error, "Connection closed"));
}

void
StratumConnection::version(const StatusCallback &onError,
                           const VersionHandler &onReply)
{
    JsonArray params;
    params.append(json_string("2.5.4")); // Our version
    params.append(json_string("0.10")); // Protocol version

    auto decoder = [onReply](JsonPtr payload) -> Status
    {
        if (!json_is_string(payload.get()))
            return ABC_ERROR(ABC_CC_JSONError, "Bad reply format");

        onReply(json_string_value(payload.get()));
        return Status();
    };

    sendMessage("server.version", params, onError, decoder);
}

void
StratumConnection::feeEstimateFetch(const StatusCallback &onError,
                                    const FeeCallback &onReply,
                                    size_t blocks)
{
    JsonArray params;
    params.append(json_integer(blocks));

    auto decoder = [onReply](JsonPtr payload) -> Status
    {
        if (!json_is_number(payload.get()))
            return ABC_ERROR(ABC_CC_JSONError, "Bad reply format");

        onReply(json_number_value(payload.get()));
        return Status();
    };

    sendMessage("blockchain.estimatefee", params, onError, decoder);
}

void
StratumConnection::sendTx(const StatusCallback &onDone, DataSlice tx)
{
    JsonArray params;
    params.append(json_string(base16Encode(tx).c_str()));

    const auto hash = bc::encode_hash(bc::bitcoin_hash(tx));
    auto decoder = [onDone, hash](JsonPtr payload) -> Status
    {
        if (!json_is_string(payload.get()))
            return ABC_ERROR(ABC_CC_Error, "Bad reply format");

        const auto message = json_string_value(payload.get());
        if (message != hash)
            return ABC_ERROR(ABC_CC_Error, message);

        onDone(Status());
        return Status();
    };

    sendMessage("blockchain.transaction.broadcast", params, onDone, decoder);
}

Status
StratumConnection::connect(const std::string &rawUri)
{
    uri_ = rawUri;

    Uri uri;
    if (!uri.decode(rawUri))
        return ABC_ERROR(ABC_CC_ParseError, "Bad URI - wrong format");

    if (stratumScheme != uri.scheme())
        return ABC_ERROR(ABC_CC_ParseError, "Bad URI - wrong scheme");

    auto server = uri.authority();
    auto last = server.find(':');
    if (std::string::npos == last)
        return ABC_ERROR(ABC_CC_ParseError, "Bad URI - no port");
    auto serverName = server.substr(0, last);
    auto serverPort = server.substr(last + 1, std::string::npos);

    // Connect to the server:
    ABC_CHECK(connection_.connect(serverName, atoi(serverPort.c_str())));
    lastKeepalive_ = std::chrono::steady_clock::now();

    return Status();
}

Status
StratumConnection::wakeup(SleepTime &sleep)
{
    // Read any data available on the socket:
    DataChunk buffer;
    ABC_CHECK(connection_.read(buffer));
    incoming_ += std::string(buffer.begin(), buffer.end());

    // Extract any incoming messages:
    while (true)
    {
        // Find the newline:
        auto where = std::find(incoming_.begin(), incoming_.end(), '\n');
        if (incoming_.end() == where)
            break;

        // Extract and process the message:
        ABC_CHECK(handleMessage(std::string(incoming_.begin(), where + 1)));
        incoming_.erase(incoming_.begin(), where + 1);
    }

    // We need to wake up every minute:
    auto now = std::chrono::steady_clock::now();
    if (lastKeepalive_ + keepaliveTime < now)
    {
        auto onError = [](Status status) { };
        auto onReply = [](const std::string &version)
        {
            ABC_DebugLog("Stratum keepalive completed");
        };
        version(onError, onReply);

        lastKeepalive_ = now;
    }
    sleep = std::chrono::duration_cast<SleepTime>(
                lastKeepalive_ + keepaliveTime - now);

    // Check the timeout:
    if (pending_.size())
    {
        if (lastProgress_ + timeout < now)
            return ABC_ERROR(ABC_CC_ServerError, "Connection timed out");
        sleep = std::min(sleep, std::chrono::duration_cast<SleepTime>(
                             lastProgress_ + timeout - now));
    }

    return Status();
}

std::string
StratumConnection::uri()
{
    return uri_;
}

bool
StratumConnection::queueFull()
{
    return 10 < pending_.size();
}

void
StratumConnection::heightSubscribe(const StatusCallback &onError,
                                   const HeightCallback &onReply)
{
    JsonPtr params;
    heightCallback_ = onReply;

    auto decoder = [onReply](JsonPtr payload) -> Status
    {
        if (!json_is_number(payload.get()))
            return ABC_ERROR(ABC_CC_Error, "Bad reply format");

        onReply(json_number_value(payload.get()));
        return Status();
    };

    sendMessage("blockchain.numblocks.subscribe", params, onError, decoder);
}

void
StratumConnection::addressHistoryFetch(const StatusCallback &onError,
                                       const AddressCallback &onReply,
                                       const std::string &address)
{
    JsonArray params;
    params.append(json_string(address.c_str()));

    auto decoder = [onReply](JsonPtr payload) -> Status
    {
        JsonArray arrayJson(payload);

        AddressHistory history;
        size_t size = arrayJson.size();
        for (size_t i = 0; i < size; i++)
        {
            struct HistoryJson:
                public JsonObject
            {
                ABC_JSON_CONSTRUCTORS(HistoryJson, JsonObject)
                ABC_JSON_STRING(txid, "tx_hash", nullptr)
                ABC_JSON_INTEGER(height, "height", 0)
            };
            HistoryJson json(arrayJson[i]);

            if (!json.txidOk())
                return ABC_ERROR(ABC_CC_Error, "Missing txid");

            history[json.txid()] = json.height();
        }

        onReply(history);
        return Status();
    };

    sendMessage("blockchain.address.get_history", params, onError, decoder);
}

void
StratumConnection::txDataFetch(const StatusCallback &onError,
                               const TxCallback &onReply,
                               const std::string &txid)
{
    JsonArray params;
    params.append(json_string(txid.c_str()));

    auto decoder = [onReply](JsonPtr payload) -> Status
    {
        if (!json_is_string(payload.get()))
            return ABC_ERROR(ABC_CC_JSONError, "Bad reply format");

        DataChunk rawTx;
        if (!base16Decode(rawTx, json_string_value(payload.get())))
            return ABC_ERROR(ABC_CC_ParseError, "Bad transaction format");
        bc::transaction_type tx;
        ABC_CHECK(decodeTx(tx, rawTx));

        onReply(tx);
        return Status();
    };

    sendMessage("blockchain.transaction.get", params, onError, decoder);
}

void
StratumConnection::blockHeaderFetch(const StatusCallback &onError,
                                    const HeaderCallback &onReply,
                                    size_t height)
{
    JsonArray params;
    params.append(json_integer(height));

    auto decoder = [onReply](JsonPtr payload) -> Status
    {
        HeaderJson headerJson(payload);

        bc::hash_digest previous_block_hash;
        bc::hash_digest merkle;
        if (!bc::decode_hash(previous_block_hash, headerJson.prev_block_hash()))
            return ABC_ERROR(ABC_CC_ParseError, "Bad hash");
        if (!bc::decode_hash(merkle, headerJson.merkle_root()))
            return ABC_ERROR(ABC_CC_ParseError, "Bad hash");

        bc::block_header_type header;
        header.previous_block_hash = previous_block_hash;
        header.merkle = merkle;
        header.version = headerJson.version();
        header.timestamp = headerJson.timestamp();
        header.bits = headerJson.bits();
        header.nonce = headerJson.nonce();

        onReply(header);
        return Status();
    };

    sendMessage("blockchain.block.get_header", params, onError, decoder);
}

void
StratumConnection::sendMessage(const std::string &method, JsonPtr params,
                               const StatusCallback &onError,
                               const Decoder &decoder)
{
    const auto id = lastId++;

    RequestJson query;
    query.idSet(id);
    query.methodSet(method);
    query.paramsSet(params);

    auto s = connection_.send(query.encode(true) + '\n');
    if (!s)
        return onError(s);

    // Start the timeout if this is the first message in the queue:
    if (pending_.empty())
        lastProgress_ = std::chrono::steady_clock::now();

    // The message has been sent, so save the decoder:
    pending_[id] = Pending{ onError, decoder };
}

Status
StratumConnection::handleMessage(const std::string &message)
{
    ReplyJson json;
    ABC_CHECK(json.decode(message));
    if (json.idOk())
    {
        auto i = pending_.find(json.id());
        if (pending_.end() != i)
        {
            auto s = i->second.decoder(json.result());
            if (!s)
                i->second.onError(s);
            pending_.erase(i);
            return Status();
        }
        else
        {
            ; // TODO: Handle mis-matched replies
        }
    }
    else
    {
        // Handle subscription updates:
        std::string method = json.method();
        if ("blockchain.numblocks.subscribe" == method && heightCallback_)
        {
            auto payload = json.params();
            if (!json_is_number(payload.get()))
                return ABC_ERROR(ABC_CC_Error, "Bad reply format");

            heightCallback_(json_number_value(payload.get()));
        }
    }

    lastProgress_ = std::chrono::steady_clock::now();
    return Status();
}

}
