/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LibbitcoinConnection.hpp"

namespace abcd {

LibbitcoinConnection::LibbitcoinConnection(void *ctx):
    queuedQueries_(0),
    socket_(std::make_shared<bc::client::zeromq_socket>(ctx)),
    codec_(socket_,
           bc::client::obelisk_router::on_update_nop,
           bc::client::obelisk_router::on_unknown_nop,
           std::chrono::seconds(10), 0)
{
}

Status
LibbitcoinConnection::connect(const std::string &uri, const std::string &key)
{
    uri_ = uri;

    if(!socket_->connect(uri_, key))
        return ABC_ERROR(ABC_CC_Error, "Could not connect to " + uri_);

    return Status();
}

std::chrono::milliseconds
LibbitcoinConnection::wakeup()
{
    std::chrono::milliseconds nextWakeup(0);
    auto now = std::chrono::steady_clock::now();

    // Figure out when our next block check is:
    if (heightCallback_)
    {
        auto period = std::chrono::seconds(30);
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                           now - lastHeightCheck_);
        if (period <= elapsed)
        {
            fetchHeight();
            lastHeightCheck_ = now;
            elapsed = std::chrono::milliseconds::zero();
        }
        nextWakeup = period - elapsed;
    }

    // Handle the socket:
    socket_->forward(codec_);
    nextWakeup = bc::client::min_sleep(nextWakeup, codec_.wakeup());

    return nextWakeup;
}

zmq_pollitem_t
LibbitcoinConnection::pollitem()
{
    return socket_->pollitem();
}

std::string
LibbitcoinConnection::uri()
{
    return uri_;
}

bool
LibbitcoinConnection::queueFull()
{
    return 10 < queuedQueries_;
}

void
LibbitcoinConnection::heightSubscribe(const StatusCallback &onError,
                                      const HeightCallback &onReply)
{
    heightError_ = onError;
    heightCallback_ = onReply;
    lastHeightCheck_ = std::chrono::steady_clock::now();

    fetchHeight();
}

void
LibbitcoinConnection::addressHistoryFetch(const StatusCallback &onError,
        const AddressCallback &onReply,
        const std::string &address)
{
    bc::payment_address parsed;
    if (!parsed.set_encoded(address))
        return onError(ABC_ERROR(ABC_CC_ParseError, "Bad address " + address));

    auto errorShim = [this, onError](const std::error_code &error)
    {
        --queuedQueries_;
        onError(ABC_ERROR(ABC_CC_Error, error.message()));
    };

    auto replyShim = [this, onReply](const bc::client::history_list &history)
    {
        --queuedQueries_;

        AddressHistory historyOut;
        for (const auto &row: history)
        {
            historyOut[bc::encode_hash(row.output.hash)] = row.output_height;
            if (row.spend.hash != bc::null_hash)
                historyOut[bc::encode_hash(row.spend.hash)] = row.spend_height;
        }
        onReply(historyOut);
    };

    ++queuedQueries_;
    codec_.address_fetch_history(errorShim, replyShim, parsed);
}

void
LibbitcoinConnection::txDataFetch(const StatusCallback &onError,
                                  const TxCallback &onReply,
                                  const std::string &txid)
{
    bc::hash_digest parsed;
    if (!bc::decode_hash(parsed, txid))
        return onError(ABC_ERROR(ABC_CC_ParseError, "Bad txid " + txid));

    auto errorShim = [this, onError](const std::error_code &error)
    {
        --queuedQueries_;
        onError(ABC_ERROR(ABC_CC_Error, error.message()));
    };

    auto replyShim = [this, onReply](const bc::transaction_type &tx)
    {
        --queuedQueries_;
        onReply(tx);
    };

    auto onErrorRetry = [this, errorShim, replyShim, parsed]
                        (const std::error_code &error)
    {
        // If that didn't work, try the mempool:
        codec_.fetch_unconfirmed_transaction(errorShim, replyShim, parsed);
    };

    ++queuedQueries_;
    codec_.fetch_transaction(onErrorRetry, replyShim, parsed);
}

void
LibbitcoinConnection::blockHeaderFetch(const StatusCallback &onError,
                                       const HeaderCallback &onReply,
                                       size_t height)
{
    auto errorShim = [this, onError](const std::error_code &error)
    {
        --queuedQueries_;
        onError(ABC_ERROR(ABC_CC_Error, error.message()));
    };

    auto replyShim = [this, onReply](const bc::block_header_type &header)
    {
        --queuedQueries_;
        onReply(header);
    };

    ++queuedQueries_;
    codec_.fetch_block_header(errorShim, replyShim, height);
}

void
LibbitcoinConnection::fetchHeight()
{
    auto errorShim = [this](const std::error_code &error)
    {
        --queuedQueries_;
        heightError_(ABC_ERROR(ABC_CC_Error, error.message()));
    };

    auto replyShim = [this](size_t height)
    {
        --queuedQueries_;
        heightCallback_(height);
    };

    ++queuedQueries_;
    codec_.fetch_last_height(errorShim, replyShim);
}

} // namespace abcd
