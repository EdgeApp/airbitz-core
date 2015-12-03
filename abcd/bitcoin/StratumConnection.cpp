/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "StratumConnection.hpp"
#include "../crypto/Encoding.hpp"
#include "../json/JsonArray.hpp"
#include "../json/JsonObject.hpp"
#include "../util/Debug.hpp"
#include <algorithm>

namespace abcd {

constexpr std::chrono::milliseconds keepaliveTime(60000);

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

void
StratumConnection::version(
    bc::client::obelisk_codec::error_handler onError,
    VersionHandler onReply)
{
    JsonArray params;
    params.append(json_string("2.5.4")); // Our version
    params.append(json_string("0.10")); // Protocol version

    RequestJson query;
    query.idSet(lastId++);
    query.methodSet("server.version");
    query.paramsSet(params);
    connection_.send(query.encode(true) + '\n');

    // Set up a decoder for the reply:
    auto decoder = [onError, onReply](ReplyJson message)
    {
        auto payload = message.result().get();
        if (!json_is_string(payload))
        {
            onError(std::make_error_code(std::errc::bad_message));
        }
        else
        {
            onReply(json_string_value(payload));
        }
    };
    pending_[query.id()] = Pending{ decoder };
}

void
StratumConnection::getTx(
    bc::client::obelisk_codec::error_handler onError,
    bc::client::obelisk_codec::fetch_transaction_handler onReply,
    const bc::hash_digest &txid)
{
    JsonArray params;
    params.append(json_string(bc::encode_hash(txid).c_str()));

    RequestJson query;
    query.idSet(lastId++);
    query.methodSet("blockchain.transaction.get");
    query.paramsSet(params);
    connection_.send(query.encode(true) + '\n');

    // Set up a decoder for the reply:
    auto decoder = [onError, onReply](ReplyJson message)
    {
        auto payload = message.result().get();
        if (!json_is_string(payload))
            return onError(std::make_error_code(std::errc::bad_message));

        bc::data_chunk rawTx;
        if (!base16Decode(rawTx, json_string_value(payload)))
            return onError(std::make_error_code(std::errc::bad_message));

        try
        {
            // Convert rawTx to bc::transaction_type:
            auto deserial = bc::make_deserializer(rawTx.begin(), rawTx.end());
            bc::transaction_type tx;
            bc::satoshi_load(deserial.iterator(), deserial.end(), tx);
            onReply(tx);
        }
        catch (bc::end_of_stream)
        {
            return onError(std::make_error_code(std::errc::bad_message));
        }
    };
    pending_[query.id()] = Pending{ decoder };
}

void
StratumConnection::getAddressHistory(
    bc::client::obelisk_codec::error_handler onError,
    bc::client::obelisk_codec::fetch_history_handler onReply,
    const bc::payment_address &address, size_t fromHeight)
{
    JsonArray params;
    params.append(json_string(address.encoded().c_str()));

    RequestJson query;
    query.idSet(lastId++);
    query.methodSet("blockchain.address.get_history");
    query.paramsSet(params);
    connection_.send(query.encode(true) + '\n');

    // Set up a decoder for the reply:
    auto decoder = [onError, onReply](ReplyJson message)
    {
        JsonArray payload(message.result());

        bc::client::history_list history;
        size_t size = payload.size();
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
            HistoryJson json(payload[i]);

            bc::hash_digest hash;
            if (!json.txidOk() || !bc::decode_hash(hash, json.txid()))
                return onError(std::make_error_code(std::errc::bad_message));

            bc::client::history_row row;
            row.output.hash = hash;
            row.output_height = json.height();
            row.spend.hash = bc::null_hash;
            history.push_back(row);
        }
        onReply(history);
    };
    pending_[query.id()] = Pending{ decoder };
}

Status
StratumConnection::connect(const std::string &hostname, int port)
{
    ABC_CHECK(connection_.connect(hostname, port));
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
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                       now - lastKeepalive_);
    if (keepaliveTime < elapsed)
    {
        auto onError = [](std::error_code ec) { };
        auto onReply = [](const std::string &version)
        {
            ABC_DebugLog("Stratum keepalive completed");
        };
        version(onError, onReply);

        lastKeepalive_ = now;
        elapsed = elapsed.zero();
    }

    sleep = keepaliveTime - elapsed;
    return Status();
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
            i->second.decoder(json);
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

    return Status();
}

}
