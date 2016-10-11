/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_NETWORK_TX_UPDATER_HPP
#define ABCD_BITCOIN_NETWORK_TX_UPDATER_HPP

#include "../Typedefs.hpp"
#include "../../util/Data.hpp"
#include "../cache/ServerCache.hpp"
#include <zmq.h>
#include <chrono>
#include <map>

namespace abcd {

class Cache;
class IBitcoinConnection;
class StratumConnection;

/**
 * Syncs a set of transactions with the bitcoin server.
 */
class TxUpdater
{
public:
    ~TxUpdater();
    TxUpdater(Cache &cache, void *ctx);

    void disconnect();
    Status connect();

    /**
     * Performs any pending work.
     * Returns the number of milliseconds until the next work will be ready.
     */
    std::chrono::milliseconds
    wakeup();

    /**
     * Obtains a list of sockets that the main loop should sleep on.
     */
    std::list<zmq_pollitem_t>
    pollitems();

    /**
     * Broadcasts a transaction.
     * All errors go to the `status` callback.
     */
    void
    sendTx(StatusCallback status, DataSlice tx);

private:
    Status connectTo(std::string server, ServerType serverType);

    Cache &cache_;
    void *ctx_;

    bool wantConnection = false;
    bool cacheDirty = false;
    time_t cacheLastSave = 0;

    std::vector<IBitcoinConnection *> connections_;
//    std::vector<std::string> serverList_;
//    std::set<int> untriedLibbitcoin_;
//    std::set<int> untriedStratum_;
    std::vector<std::string> stratumServers_;
    std::vector<std::string> libbitcoinServers_;

    // Fetches currently in progress:
    AddressSet wipAddresses_;
    TxidSet wipTxids_;

    /**
     * The last server used to query the address.
     * Used to avoid reusing the same server over and over,
     * and to fetch transactions from the same server that reported them.
     */
    std::map<std::string, std::string> addressServers_;

    /**
     * A list of servers that have failed.
     */
    std::set<std::string> failedServers_;

    /**
     * Finds the requested server, assuming it is even connected and ready.
     * @return The best available server,
     * or a null pointer if the server is busy.
     */
    IBitcoinConnection *
    pickServer(const std::string &name);

    /**
     * Tries to pick a different server than the one provided.
     * @return The best available server,
     * or a null pointer if there are no free servers.
     */
    IBitcoinConnection *
    pickOtherServer(const std::string &name="");

    void
    subscribeHeight(IBitcoinConnection *bc);

    void
    subscribeAddress(const std::string &address, IBitcoinConnection *bc);

    void
    fetchAddress(const std::string &address, IBitcoinConnection *bc);

    void
    fetchTx(const std::string &txid, IBitcoinConnection *bc);

    void
    fetchFeeEstimate(size_t blocks, StratumConnection *sc);

    void
    blockHeaderFetch(size_t height, IBitcoinConnection *bc);
};

} // namespace abcd

#endif
