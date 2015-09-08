/*
 * Copyright (c) 2011-2014 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin.
 *
 * libbitcoin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License with
 * additional permissions to the one published by the Free Software
 * Foundation, either version 3 of the License, or (at your option)
 * any later version. For more information see LICENSE.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#include "obelisk_codec.hpp"

namespace libbitcoin {
namespace client {

using std::placeholders::_1;

BC_API obelisk_codec::obelisk_codec(message_stream& out,
    unknown_handler&& on_unknown,
    sleep_time timeout, unsigned retries)
  : next_part_(command_part), last_request_id_(0),
    timeout_(timeout), retries_(retries),
    on_unknown_(std::move(on_unknown)),
    out_(out)
{
}

BC_API void obelisk_codec::message(const data_chunk& data, bool more)
{
    switch (next_part_)
    {
        case command_part:
            wip_message_.command = std::string(data.begin(), data.end());
            break;

        case id_part:
            if (4 != data.size())
            {
                next_part_ = error_part;
                break;
            }
            wip_message_.id = from_little_endian<uint32_t>(data.begin());
            break;

        case payload_part:
            wip_message_.payload = data;
            break;

        case error_part:
            break;
    }
    if (!more)
    {
        if (next_part_ == payload_part)
            receive(wip_message_);
        else
            on_unknown_(wip_message_.command);
        next_part_ = command_part;
    }
    else if (next_part_ < error_part)
        next_part_ = static_cast<message_part>(next_part_ + 1);
}

BC_API sleep_time obelisk_codec::wakeup()
{
    sleep_time next_wakeup(0);
    auto now = std::chrono::steady_clock::now();

    auto i = pending_requests_.begin();
    while (i != pending_requests_.end())
    {
        auto request = i++;
        auto elapsed = std::chrono::duration_cast<sleep_time>(
            now - request->second.last_action);
        if (timeout_ <= elapsed)
        {
            if (request->second.retries < retries_)
            {
                // Resend:
                ++request->second.retries;
                request->second.last_action = now;
                next_wakeup = min_sleep(next_wakeup, timeout_);
                send(request->second.message);
            }
            else
            {
                // Cancel:
                auto ec = std::make_error_code(std::errc::timed_out);
                request->second.on_error(ec);
                pending_requests_.erase(request);
            }
        }
        else
            next_wakeup = min_sleep(next_wakeup, timeout_ - elapsed);
    }
    return next_wakeup;
}

BC_API void obelisk_codec::fetch_history(error_handler&& on_error,
    fetch_history_handler&& on_reply,
    const payment_address& address, size_t from_height)
{
    data_chunk data;
    data.resize(1 + short_hash_size + 4);
    auto serial = make_serializer(data.begin());
    serial.write_byte(address.version());
    serial.write_short_hash(address.hash());
    serial.write_4_bytes(from_height);
    BITCOIN_ASSERT(serial.iterator() == data.end());

    send_request("blockchain.fetch_history", data, std::move(on_error),
        std::bind(decode_fetch_history, _1, std::move(on_reply)));
}

BC_API void obelisk_codec::fetch_transaction(error_handler&& on_error,
    fetch_transaction_handler&& on_reply,
    const hash_digest& tx_hash)
{
    data_chunk data;
    data.resize(hash_size);
    auto serial = make_serializer(data.begin());
    serial.write_hash(tx_hash);
    BITCOIN_ASSERT(serial.iterator() == data.end());

    send_request("blockchain.fetch_transaction", data, std::move(on_error),
        std::bind(decode_fetch_transaction, _1, std::move(on_reply)));
}

BC_API void obelisk_codec::fetch_last_height(error_handler&& on_error,
    fetch_last_height_handler&& on_reply)
{
    send_request("blockchain.fetch_last_height", data_chunk(),
        std::move(on_error),
        std::bind(decode_fetch_last_height, _1, std::move(on_reply)));
}

BC_API void obelisk_codec::fetch_block_header(error_handler&& on_error,
    fetch_block_header_handler&& on_reply,
    size_t height)
{
    data_chunk data = to_data_chunk(to_little_endian<uint32_t>(height));

    send_request("blockchain.fetch_block_header", data, std::move(on_error),
        std::bind(decode_fetch_block_header, _1, std::move(on_reply)));
}

BC_API void obelisk_codec::fetch_block_header(error_handler&& on_error,
    fetch_block_header_handler&& on_reply,
    const hash_digest& blk_hash)
{
    data_chunk data(hash_size);
    auto serial = make_serializer(data.begin());
    serial.write_hash(blk_hash);
    BITCOIN_ASSERT(serial.iterator() == data.end());

    send_request("blockchain.fetch_block_header", data, std::move(on_error),
        std::bind(decode_fetch_block_header, _1, std::move(on_reply)));
}

BC_API void obelisk_codec::fetch_transaction_index(error_handler&& on_error,
    fetch_transaction_index_handler&& on_reply,
    const hash_digest& tx_hash)
{
    data_chunk data(hash_size);
    auto serial = make_serializer(data.begin());
    serial.write_hash(tx_hash);
    BITCOIN_ASSERT(serial.iterator() == data.end());

    send_request("blockchain.fetch_transaction_index", data,
        std::move(on_error),
        std::bind(decode_fetch_transaction_index, _1, std::move(on_reply)));
}

BC_API void obelisk_codec::validate(error_handler&& on_error,
    validate_handler&& on_reply,
    const transaction_type& tx)
{
    data_chunk data(satoshi_raw_size(tx));
    auto it = satoshi_save(tx, data.begin());
    BITCOIN_ASSERT(it == data.end());

    send_request("transaction_pool.validate", data, std::move(on_error),
        std::bind(decode_validate, _1, std::move(on_reply)));
}

BC_API void obelisk_codec::fetch_unconfirmed_transaction(
    error_handler&& on_error,
    fetch_transaction_handler&& on_reply,
    const hash_digest& tx_hash)
{
    data_chunk data;
    data.resize(hash_size);
    auto serial = make_serializer(data.begin());
    serial.write_hash(tx_hash);
    BITCOIN_ASSERT(serial.iterator() == data.end());

    send_request("transaction_pool.fetch_transaction", data,
        std::move(on_error),
        std::bind(decode_fetch_transaction, _1, std::move(on_reply)));
}

BC_API void obelisk_codec::broadcast_transaction(error_handler&& on_error,
    empty_handler&& on_reply,
    const transaction_type& tx)
{
    data_chunk data(satoshi_raw_size(tx));
    auto it = satoshi_save(tx, data.begin());
    BITCOIN_ASSERT(it == data.end());

    send_request("protocol.broadcast_transaction", data, std::move(on_error),
        std::bind(decode_empty, _1, std::move(on_reply)));
}

BC_API void obelisk_codec::address_fetch_history(error_handler&& on_error,
    fetch_history_handler&& on_reply,
    const payment_address& address, size_t from_height)
{
    data_chunk data;
    data.resize(1 + short_hash_size + 4);
    auto serial = make_serializer(data.begin());
    serial.write_byte(address.version());
    serial.write_short_hash(address.hash());
    serial.write_4_bytes(from_height);
    BITCOIN_ASSERT(serial.iterator() == data.end());

    send_request("address.fetch_history", data, std::move(on_error),
        std::bind(decode_fetch_history, _1, std::move(on_reply)));
}

void obelisk_codec::decode_empty(data_deserial& payload,
    empty_handler& handler)
{
    check_end(payload);
    handler();
}

void obelisk_codec::decode_fetch_history(data_deserial& payload,
    fetch_history_handler& handler)
{
    blockchain::history_list history;
    while (payload.iterator() != payload.end())
    {
        blockchain::history_row row;
        row.output.hash = payload.read_hash();
        row.output.index = payload.read_4_bytes();
        row.output_height = payload.read_4_bytes();
        row.value = payload.read_8_bytes();
        row.spend.hash = payload.read_hash();
        row.spend.index = payload.read_4_bytes();
        row.spend_height = payload.read_4_bytes();
        history.push_back(row);
    }
    handler(history);
}

void obelisk_codec::decode_fetch_transaction(data_deserial& payload,
    fetch_transaction_handler& handler)
{
    transaction_type tx;
    satoshi_load(payload.iterator(), payload.end(), tx);
    payload.set_iterator(payload.iterator() + satoshi_raw_size(tx));
    check_end(payload);
    handler(tx);
}

void obelisk_codec::decode_fetch_last_height(data_deserial& payload,
    fetch_last_height_handler& handler)
{
    size_t last_height = payload.read_4_bytes();
    check_end(payload);
    handler(last_height);
}

void obelisk_codec::decode_fetch_block_header(data_deserial& payload,
    fetch_block_header_handler& handler)
{
    block_header_type header;
    satoshi_load(payload.iterator(), payload.end(), header);
    payload.set_iterator(payload.iterator() + satoshi_raw_size(header));
    check_end(payload);
    handler(header);
}

void obelisk_codec::decode_fetch_transaction_index(data_deserial& payload,
    fetch_transaction_index_handler& handler)
{
    uint32_t block_height = payload.read_4_bytes();
    uint32_t index = payload.read_4_bytes();
    check_end(payload);
    handler(block_height, index);
}

void obelisk_codec::decode_validate(data_deserial& payload,
    validate_handler& handler)
{
    index_list unconfirmed;
    while (payload.iterator() != payload.end())
    {
        size_t unconfirm_index = payload.read_4_bytes();
        unconfirmed.push_back(unconfirm_index);
    }
    handler(unconfirmed);
}

void obelisk_codec::check_end(data_deserial& payload)
{
    if (payload.iterator() != payload.end())
        throw end_of_stream();
}

void obelisk_codec::send_request(const std::string& command,
    const data_chunk& payload,
    error_handler&& on_error, decoder&& on_reply)
{
    uint32_t id = ++last_request_id_;
    pending_request& request = pending_requests_[id];
    request.message = obelisk_message{command, id, payload};
    request.on_error = std::move(on_error);
    request.on_reply = std::move(on_reply);
    request.retries = 0;
    request.last_action = std::chrono::steady_clock::now();
    send(request.message);
}

void obelisk_codec::send(const obelisk_message& message)
{
    out_.message(to_data_chunk(message.command), true);
    out_.message(to_data_chunk(to_little_endian(message.id)), true);
    out_.message(message.payload, false);
}

void obelisk_codec::receive(const obelisk_message& message)
{
    auto i = pending_requests_.find(message.id);
    if (i == pending_requests_.end())
    {
        on_unknown_(message.command);
        return;
    }
    decode_reply(message, i->second.on_error, i->second.on_reply);
    pending_requests_.erase(i);
}

void obelisk_codec::decode_reply(const obelisk_message& message,
    error_handler& on_error, decoder& on_reply)
{
    std::error_code ec;
    data_deserial deserial = make_deserializer(
        message.payload.begin(), message.payload.end());
    try
    {
        uint32_t val = deserial.read_4_bytes();
        if (val)
            ec = static_cast<error::error_code_t>(val);
        else
            on_reply(deserial);
    }
    catch (end_of_stream)
    {
        ec = std::make_error_code(std::errc::bad_message);
    }
    if (ec)
        on_error(ec);
}

BC_API void obelisk_codec::on_unknown_nop(const std::string&)
{
}

} // namespace client
} // namespace libbitcoin
