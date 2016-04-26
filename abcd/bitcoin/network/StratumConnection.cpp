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

struct ReplyJson:
    public JsonObject
{
    ABC_JSON_INTEGER(id, "id", 0)
    ABC_JSON_VALUE(result, "result", JsonPtr);
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
StratumConnection::getTx(
    const bc::client::obelisk_codec::error_handler &onError,
    const bc::client::obelisk_codec::fetch_transaction_handler &onReply,
    const bc::hash_digest &txid)
{
    JsonArray params;
    params.append(json_string(bc::encode_hash(txid).c_str()));

    auto errorShim = [onError](Status status)
    {
        onError(std::make_error_code(std::errc::bad_message));
    };

    auto decoder = [onReply](JsonPtr payload) -> Status
    {
        if (!json_is_string(payload.get()))
            return ABC_ERROR(ABC_CC_JSONError, "Bad reply format");

        bc::data_chunk rawTx;
        if (!base16Decode(rawTx, json_string_value(payload.get())))
            return ABC_ERROR(ABC_CC_ParseError, "Bad transaction format");

        // Convert rawTx to bc::transaction_type:
        bc::transaction_type tx;
        ABC_CHECK(decodeTx(tx, rawTx));

        onReply(tx);
        return Status();
    };

    sendMessage("blockchain.transaction.get", params, errorShim, decoder);
}

void
StratumConnection::getAddressHistory(
    const bc::client::obelisk_codec::error_handler &onError,
    const bc::client::obelisk_codec::fetch_history_handler &onReply,
    const bc::payment_address &address, size_t fromHeight)
{
    JsonArray params;
    params.append(json_string(address.encoded().c_str()));

    auto errorShim = [onError](Status status)
    {
        onError(std::make_error_code(std::errc::bad_message));
    };

    auto decoder = [onReply](JsonPtr payload) -> Status
    {
        JsonArray arrayJson(payload);

        bc::client::history_list history;
        size_t size = arrayJson.size();
        history.reserve(size);
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

            bc::hash_digest hash;
            if (!json.txidOk() || !bc::decode_hash(hash, json.txid()))
                return ABC_ERROR(ABC_CC_Error, "Bad txid");

            bc::client::history_row row;
            row.output.hash = hash;
            row.output_height = json.height();
            row.spend.hash = bc::null_hash;
            history.push_back(row);
        }

        onReply(history);
        return Status();
    };

    sendMessage("blockchain.address.get_history", params, errorShim, decoder);
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

void
StratumConnection::getHeight(
    const bc::client::obelisk_codec::error_handler &onError,
    const HeightHandler &onReply)
{
    auto errorShim = [onError](Status status)
    {
        onError(std::make_error_code(std::errc::bad_message));
    };

    auto decoder = [onReply](JsonPtr payload) -> Status
    {
        if (!json_is_number(payload.get()))
            return ABC_ERROR(ABC_CC_Error, "Bad reply format");

        onReply(json_number_value(payload.get()));
        return Status();
    };

    sendMessage("blockchain.numblocks.subscribe", JsonPtr(),
                errorShim, decoder);
}

Status
StratumConnection::connect(const std::string &rawUri)
{
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
        ; // TODO: Handle subscription updates
    }

    lastProgress_ = std::chrono::steady_clock::now();
    return Status();
}

}
