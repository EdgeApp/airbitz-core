/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_NETWORK_LIBBITCOIN_CONNECTION_HPP
#define ABCD_BITCOIN_NETWORK_LIBBITCOIN_CONNECTION_HPP

#include "IBitcoinConnection.hpp"
#include "../../../minilibs/libbitcoin-client/client.hpp"

namespace abcd {

/**
 * Wraps a libbitcoin connection in the `IBitcoinConnection` interface.
 */
class LibbitcoinConnection:
    public bc::client::sleeper,
    public IBitcoinConnection
{
public:
    LibbitcoinConnection(void *ctx);

    Status connect(const std::string &uri, const std::string &key);
    zmq_pollitem_t pollitem();

    // Sleeper interface:
    bc::client::sleep_time wakeup() override;

    // IBitcoinConnection interface:
    std::string
    uri() override;

    bool
    queueFull() override;

    void
    heightSubscribe(const StatusCallback &onError,
                    const HeightCallback &onReply) override;

    void
    addressHistoryFetch(const StatusCallback &onError,
                        const AddressCallback &onReply,
                        const std::string &address) override;

    void
    txDataFetch(const StatusCallback &onError,
                const TxCallback &onReply,
                const std::string &txid) override;

    void
    blockHeaderFetch(const StatusCallback &onError,
                     const HeaderCallback &onReply,
                     size_t height) override;

private:
    // Connection:
    std::string uri_;
    int queuedQueries_;

    // Height-check state:
    StatusCallback heightError_;
    HeightCallback heightCallback_;
    std::chrono::steady_clock::time_point lastHeightCheck_;

    // The actual obelisk connection (destructor called first):
    bc::client::zeromq_socket socket_;
    bc::client::obelisk_codec codec_;

    void
    fetchHeight();
};

} // namespace abcd

#endif
