/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_STRATUM_CODEC_HPP
#define ABCD_BITCOIN_STRATUM_CODEC_HPP

#include "TcpConnection.hpp"
#include "../../minilibs/libbitcoin-client/client.hpp"
#include <chrono>
#include <functional>
#include <string>

namespace abcd {

struct ReplyJson;
typedef std::chrono::milliseconds SleepTime;

class StratumConnection
{
public:
    typedef std::function<void (const std::string &version)> VersionHandler;
    typedef std::function<void (const size_t &height)> HeightHandler;
    typedef std::function<void(Status)> StatusCallback;

    /**
     * Requests the server version.
     */
    void version(
        bc::client::obelisk_codec::error_handler onError,
        VersionHandler onReply);

    /**
     * Requests a transaction from the server.
     */
    void getTx(
        bc::client::obelisk_codec::error_handler onError,
        bc::client::obelisk_codec::fetch_transaction_handler onReply,
        const bc::hash_digest &txid);

    void getAddressHistory(
        bc::client::obelisk_codec::error_handler onError,
        bc::client::obelisk_codec::fetch_history_handler onReply,
        const bc::payment_address &address, size_t fromHeight=0);

    Status sendTx(StatusCallback status, DataSlice tx);

    /**
     * Requests current blockchain height from the server.
     */
    void
    getHeight(
        bc::client::obelisk_codec::error_handler onError,
        HeightHandler onReply);

    /**
     * Connects to the specified stratum server.
     */
    Status
    connect(const std::string &hostname, int port);

    /**
     * Performs any pending work,
     * and returns the number of ms until the next time we need a wakeup.
     */
    Status
    wakeup(SleepTime &sleep);

    /**
     * Obtains the socket that the main loop should sleep on.
     */
    int pollfd() const { return connection_.pollfd(); }

private:
    // Socket:
    TcpConnection connection_;
    std::string incoming_;

    // Sending:
    unsigned lastId = 0;
    struct Pending
    {
        std::function<void (ReplyJson message)> decoder;
        // TODO: Add timeout logic
    };
    std::map<unsigned, Pending> pending_;

    // Server heartbeat:
    std::chrono::steady_clock::time_point lastKeepalive_;

    /**
     * Decodes and handles a complete message from the server.
     */
    Status
    handleMessage(const std::string &message);
};

} // namespace abcd

#endif
