/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_NETWORK_STRATUM_CODEC_HPP
#define ABCD_BITCOIN_NETWORK_STRATUM_CODEC_HPP

#include "IBitcoinConnection.hpp"
#include "TcpConnection.hpp"
#include <chrono>
#include <map>

namespace abcd {

class JsonPtr;
typedef std::chrono::milliseconds SleepTime;

// Scheme used for stratum URI's:
constexpr auto stratumScheme = "stratum";

class StratumConnection:
    public IBitcoinConnection
{
public:
    typedef std::function<void (const std::string &version)> VersionHandler;
    typedef std::function<void (double fee)> FeeCallback;

    ~StratumConnection();

    /**
     * Requests the server version.
     */
    void
    version(const StatusCallback &onError, const VersionHandler &onReply);

    /**
     * Fetches an estimate of the mining fees needed
     * to confirm a transaction in the given number of blocks.
     */
    void
    feeEstimateFetch(const StatusCallback &onError,
                     const FeeCallback &onReply,
                     size_t blocks);

    /**
     * Broadcasts a transaction over the Bitcoin network.
     * @param onDone called when the broadcast is done,
     * either successful or failed.
     */
    void
    sendTx(const StatusCallback &onDone, DataSlice tx);

    /**
     * Connects to the specified stratum server.
     */
    Status
    connect(const std::string &uri);

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
    typedef std::function<Status (JsonPtr payload)> Decoder;

    // Socket:
    std::string uri_;
    TcpConnection connection_;
    std::string incoming_;

    // Sending:
    unsigned lastId = 0;
    struct Pending
    {
        StatusCallback onError;
        Decoder decoder;
        // TODO: Add timeout logic
    };
    std::map<unsigned, Pending> pending_;

    // Timeout:
    std::chrono::steady_clock::time_point lastProgress_;

    // Server heartbeat:
    std::chrono::steady_clock::time_point lastKeepalive_;

    // Subscriptions:
    HeightCallback heightCallback_;

    /**
     * Sends a message and sets up the reply decoder.
     * If anything goes wrong (including errors returned by the decoder),
     * the error callback will be called.
     */
    void
    sendMessage(const std::string &method, JsonPtr params,
                const StatusCallback &onError, const Decoder &decoder);

    /**
     * Decodes and handles a complete message from the server.
     */
    Status
    handleMessage(const std::string &message);
};

} // namespace abcd

#endif
