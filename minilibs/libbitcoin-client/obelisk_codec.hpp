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
#ifndef LIBBITCOIN_CLIENT_OBELISK_CODEC_HPP
#define LIBBITCOIN_CLIENT_OBELISK_CODEC_HPP

#include <functional>
#include <map>
#include "message_stream.hpp"
#include "sleeper.hpp"

namespace libbitcoin {
namespace client {

struct history_row
{
    output_point output;
    size_t output_height;
    uint64_t value;
    input_point spend;
    size_t spend_height;
};

typedef std::vector<history_row> history_list;

/**
 * Decodes and encodes messages in the obelisk protocol.
 * This class is a pure codec; it does not talk directly to zeromq.
 */
class BC_API obelisk_codec
  : public message_stream, public sleeper
{
public:
    // Loose message handlers:
    typedef std::function<void (const std::string& command)> unknown_handler;

    /**
     * Constructor.
     * @param out a stream to receive outgoing messages created by the codec.
     * @param on_update function to handle subscription update messages.
     * @param on_unknown function to handle malformed incoming messages.
     */
    BC_API obelisk_codec(message_stream& out,
        unknown_handler&& on_unknown=on_unknown_nop,
        sleep_time timeout=std::chrono::seconds(2),
        unsigned retries=1);

    /**
     * Pass in a message for decoding.
     */
    BC_API void message(const data_chunk& data, bool more);

    // sleeper interface:
    BC_API sleep_time wakeup();

    // Message reply handlers:
    typedef std::function<void (const std::error_code&)>
        error_handler;
    typedef std::function<void (const history_list&)>
        fetch_history_handler;
    typedef std::function<void (const transaction_type&)>
        fetch_transaction_handler;
    typedef std::function<void (size_t)>
        fetch_last_height_handler;
    typedef std::function<void (const block_header_type&)>
        fetch_block_header_handler;
    typedef std::function<void (size_t block_height, size_t index)>
        fetch_transaction_index_handler;
    typedef std::function<void (const index_list& unconfirmed)>
        validate_handler;
    typedef std::function<void ()> empty_handler;

    // Outgoing messages:
    BC_API void fetch_history(error_handler&& on_error,
        fetch_history_handler&& on_reply,
        const payment_address& address, size_t from_height=0);
    BC_API void fetch_transaction(error_handler&& on_error,
        fetch_transaction_handler&& on_reply,
        const hash_digest& tx_hash);
    BC_API void fetch_last_height(error_handler&& on_error,
        fetch_last_height_handler&& on_reply);
    BC_API void fetch_block_header(error_handler&& on_error,
        fetch_block_header_handler&& on_reply,
        size_t height);
    BC_API void fetch_block_header(error_handler&& on_error,
        fetch_block_header_handler&& on_reply,
        const hash_digest& blk_hash);
    BC_API void fetch_transaction_index(error_handler&& on_error,
        fetch_transaction_index_handler&& on_reply,
        const hash_digest& tx_hash);
    BC_API void validate(error_handler&& on_error,
        validate_handler&& on_reply,
        const transaction_type& tx);
    BC_API void fetch_unconfirmed_transaction(error_handler&& on_error,
        fetch_transaction_handler&& on_reply,
        const hash_digest& tx_hash);
    BC_API void broadcast_transaction(error_handler&& on_error,
        empty_handler&& on_reply,
        const transaction_type& tx);
    BC_API void address_fetch_history(error_handler&& on_error,
        fetch_history_handler&& on_reply,
        const payment_address& address, size_t from_height=0);

private:
    typedef deserializer<data_chunk::const_iterator, true> data_deserial;

    /**
     * Decodes a message and calls the appropriate callback.
     * By the time this is called, the error code has already been read
     * out of the payload and checked.
     * If there is something wrong with the payload, this function should
     * throw a end_of_stream exception.
     */
    typedef std::function<void (data_deserial& payload)> decoder;
    static void decode_empty(data_deserial& payload,
        empty_handler& handler);
    static void decode_fetch_history(data_deserial& payload,
        fetch_history_handler& handler);
    static void decode_fetch_transaction(data_deserial& payload,
        fetch_transaction_handler& handler);
    static void decode_fetch_last_height(data_deserial& payload,
        fetch_last_height_handler& handler);
    static void decode_fetch_block_header(data_deserial& payload,
        fetch_block_header_handler& handler);
    static void decode_fetch_transaction_index(data_deserial& payload,
        fetch_transaction_index_handler& handler);
    static void decode_validate(data_deserial& payload,
        validate_handler& handler);

    /**
     * Verifies that the deserializer has reached the end of the payload,
     * and throws end_of_stream if not.
     */
    static void check_end(data_deserial& payload);

    /**
     * Sends an outgoing request, and adds the handlers to the pending
     * request table.
     */
    void send_request(const std::string& command,
        const data_chunk& payload,
        error_handler&& on_error, decoder&& on_reply);

    struct obelisk_message
    {
        std::string command;
        uint32_t id;
        data_chunk payload;
    };
    void send(const obelisk_message& message);
    void receive(const obelisk_message& message);
    void decode_reply(const obelisk_message& message,
        error_handler& on_error, decoder& on_reply);

    BC_API static void on_unknown_nop(const std::string&);

    // Incoming message assembly:
    obelisk_message wip_message_;
    enum message_part
    {
        command_part,
        id_part,
        payload_part,
        error_part
    };
    message_part next_part_;

    // Request management:
    uint32_t last_request_id_;
    struct pending_request
    {
        obelisk_message message;
        error_handler on_error;
        decoder on_reply;
        unsigned retries;
        std::chrono::steady_clock::time_point last_action;
    };
    std::map<uint32_t, pending_request> pending_requests_;

    // Timeout parameters:
    sleep_time timeout_;
    unsigned retries_;

    // Loose-message event handlers:
    unknown_handler on_unknown_;

    // Outgoing message stream:
    message_stream& out_;
};

} // namespace client
} // namespace libbitcoin

#endif

