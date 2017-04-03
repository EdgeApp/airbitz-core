/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "TxUpdater.hpp"
#include "LibbitcoinConnection.hpp"
#include "StratumConnection.hpp"
#include "../cache/Cache.hpp"
#include "../../General.hpp"
#include "../../util/Debug.hpp"
#include <sys/time.h>

namespace abcd {

constexpr auto NUM_CONNECT_SERVERS = 5;
constexpr auto MINIMUM_LIBBITCOIN_SERVERS = 1;
constexpr auto MINIMUM_STRATUM_SERVERS = 4;

TxUpdater::~TxUpdater()
{
    disconnect();
}

TxUpdater::TxUpdater(Cache &cache, void *ctx):
    cache_(cache),
    ctx_(ctx)
{
}

void
TxUpdater::disconnect()
{
    wantConnection = false;

    auto i = connections_.begin();
    while (i != connections_.end())
    {
        delete *i;
        i = connections_.erase(i);
    }

    ABC_DebugLog("Disconnected from all servers.");
}

Status
TxUpdater::connect()
{
    wantConnection = true;

    // If we are out of fresh stratum servers, reload the list:
    if (stratumServers_.empty())
        stratumServers_ = cache_.servers.getServers(ServerTypeStratum,
                          MINIMUM_STRATUM_SERVERS * 2);

    // If we are out of fresh libbitcoin servers, reload the list:
    if (libbitcoinServers_.empty())
        libbitcoinServers_ = cache_.servers.getServers(ServerTypeLibbitcoin,
                             MINIMUM_LIBBITCOIN_SERVERS * 2);

    for (int i = 0; i < libbitcoinServers_.size(); i++)
        ABC_DebugLevel(1, "libbitcoinServers_[%d]=%s", i,
                       libbitcoinServers_[i].c_str());
    for (int i = 0; i < stratumServers_.size(); i++)
        ABC_DebugLevel(1, "stratumServers_[%d]=%s", i, stratumServers_[i].c_str());

    ABC_DebugLevel(2,"%d libbitcoin untried, %d stratrum untried",
                   libbitcoinServers_.size(), stratumServers_.size());

    // Count the number of existing connections:
    size_t stratumCount = 0;
    size_t libbitcoinCount = 0;
    for (auto *bc: connections_)
    {
        if (dynamic_cast<StratumConnection *>(bc))
            ++stratumCount;
        if (dynamic_cast<LibbitcoinConnection *>(bc))
            ++libbitcoinCount;
    }

    // Let's make some connections:
    srand(time(nullptr));
    int numConnections = 0;
    while (connections_.size() < NUM_CONNECT_SERVERS
            && (libbitcoinServers_.size() || stratumServers_.size()))
    {
        auto *untriedPrimary = &stratumServers_;
        auto *primaryCount = &stratumCount;
        auto *untriedSecondary = &libbitcoinServers_;
        auto *secondaryCount = &libbitcoinCount;
        long minPrimary = MINIMUM_STRATUM_SERVERS;
        long minSecondary = MINIMUM_LIBBITCOIN_SERVERS;
        ServerType primaryType = ServerTypeStratum;
        ServerType secondaryType = ServerTypeLibbitcoin;

        if (numConnections % 2 == 1)
        {
            untriedPrimary = &libbitcoinServers_;
            untriedSecondary = &stratumServers_;
            primaryCount = &libbitcoinCount;
            secondaryCount = &stratumCount;
            minPrimary = MINIMUM_LIBBITCOIN_SERVERS;
            minSecondary = MINIMUM_STRATUM_SERVERS;
            primaryType = ServerTypeLibbitcoin;
            secondaryType = ServerTypeStratum;
        }

        if (untriedPrimary->size() &&
                ((minSecondary - *secondaryCount < NUM_CONNECT_SERVERS - connections_.size()) ||
                 (rand() & 8)))
        {
            auto i = untriedPrimary->begin();
            std::advance(i, rand() % untriedPrimary->size());
            if (connectTo(*i, primaryType).log())
            {
                (*primaryCount)++;
                ++numConnections;
            }
            else
            {
                cache_.servers.serverScoreDown(*i);
            }
            untriedPrimary->erase(i);
        }
        else if (untriedSecondary->size() &&
                 ((minPrimary - *primaryCount < NUM_CONNECT_SERVERS - connections_.size()) ||
                  (rand() & 8)))
        {
            auto i = untriedSecondary->begin();
            std::advance(i, rand() % untriedSecondary->size());
            if (connectTo(*i, secondaryType).log())
            {
                (*secondaryCount)++;
                ++numConnections;
            }
            else
            {
                cache_.servers.serverScoreDown(*i);
            }
            untriedSecondary->erase(i);
        }
    }

    return Status();
}

std::chrono::milliseconds
TxUpdater::wakeup()
{
    // Handle any old work that has finished:
    std::chrono::milliseconds nextWakeup(0);
    for (auto *bc: connections_)
    {
        auto *sc = dynamic_cast<StratumConnection *>(bc);
        if (sc)
        {
            SleepTime sleep;
            if (!sc->wakeup(sleep).log())
                failedServers_.insert(bc->uri());
            else
            {
                cache_.servers.serverScoreUp(bc->uri(), 0);
                nextWakeup = bc::client::min_sleep(nextWakeup, sleep);
            }
        }

        auto *lc = dynamic_cast<LibbitcoinConnection *>(bc);
        if (lc)
            nextWakeup = bc::client::min_sleep(nextWakeup, lc->wakeup());
    }

    // Fetch missing transactions:
    time_t sleep;
    const auto statuses = cache_.addresses.statuses(sleep);
    nextWakeup = bc::client::min_sleep(nextWakeup, std::chrono::seconds(sleep));
    for (const auto &status: statuses)
    {
        for (const auto &txid: status.missingTxids)
        {
            // Try to use the same server:
            auto *bc = pickServer(addressServers_[status.address]);
            if (!bc)
                break;

            fetchTx(txid, bc);
        }
    }

    // Schedule new address work:
    for (const auto &status: statuses)
    {
        if (status.dirty)
        {
            // Try to use the same server that made us dirty:
            auto *bc = pickServer(addressServers_[status.address]);
            if (!bc)
                break;

            if (bc->addressSubscribed(status.address))
                fetchAddress(status.address, bc);
            else
                subscribeAddress(status.address, bc);
        }
        else if (status.needsCheck)
        {
            // Try to use a different server than last time:
            auto *bc = pickOtherServer(addressServers_[status.address]);
            if (!bc)
                break;

            subscribeAddress(status.address, bc);
        }
    }

    // Grab block headers that we don't have:
    while (true)
    {
        size_t headerNeeded = cache_.blocks.headerNeeded();
        if (!headerNeeded)
            break;

        auto *bc = pickOtherServer();
        if (!bc)
            break;

        blockHeaderFetch(headerNeeded, bc);
    }
    cache_.blocks.save();
    cache_.blocks.onHeaderInvoke();
    cache_.servers.serverCacheSave();

    // Save the cache if it is dirty and enough time has elapsed:
    if (cacheDirty)
    {
        time_t now = time(nullptr);

        if (10 <= now - cacheLastSave)
        {
            cache_.save().log(); // Failure is fine
            cacheLastSave = now;
            cacheDirty = false;
        }
    }

    // Prune failed servers:
    for (const auto &uri: failedServers_)
    {
        auto i = connections_.begin();
        while (i != connections_.end())
        {
            auto *bc = *i;
            if (uri == bc->uri())
            {
                ABC_DebugLog("Disconnecting from %s", bc->uri().c_str());
                cache_.servers.serverScoreDown(bc->uri());
                delete bc;
                i = connections_.erase(i);
            }
            else
            {
                ++i;
            }
        }
    }
    failedServers_.clear();

    // Connect to more servers:
    if (wantConnection && connections_.size() < NUM_CONNECT_SERVERS)
        connect().log();

    return nextWakeup;
}

std::list<zmq_pollitem_t>
TxUpdater::pollitems()
{
    std::list<zmq_pollitem_t> out;
    for (auto *bc: connections_)
    {
        auto *sc = dynamic_cast<StratumConnection *>(bc);
        if (sc)
        {
            zmq_pollitem_t pollitem =
            {
                nullptr, sc->pollfd(), ZMQ_POLLIN, ZMQ_POLLOUT
            };
            out.push_back(pollitem);
        }

        auto *lc = dynamic_cast<LibbitcoinConnection *>(bc);
        if (lc)
            out.push_back(lc->pollitem());
    }
    return out;
}

void
TxUpdater::sendTx(StatusCallback status, DataSlice tx)
{
    for (auto *bc: connections_)
    {
        // Pick one (and only one) stratum server for the broadcast:
        auto *sc = dynamic_cast<StratumConnection *>(bc);
        if (sc)
        {
            sc->sendTx(status, tx);
            return;
        }
    }

    // If we get here, there are no stratum connections:
    status(ABC_ERROR(ABC_CC_Error, "No stratum connections"));
}

Status
TxUpdater::connectTo(std::string server, ServerType serverType)
{
    std::string key;

    // Parse out the key part:
    size_t keyStart = server.find(' ');
    if (keyStart != std::string::npos)
    {
        key = server.substr(keyStart + 1);
        server.erase(keyStart);
    }

    // Make the connection:
    std::unique_ptr<IBitcoinConnection> bc;
    if (ServerTypeLibbitcoin == serverType)
    {
        // Libbitcoin server:
        std::unique_ptr<LibbitcoinConnection> lc(new LibbitcoinConnection(ctx_));
        ABC_CHECK(lc->connect(server, key));
        bc.reset(lc.release());
    }
    else if (ServerTypeStratum == serverType)
    {
        // Stratum server:
        std::unique_ptr<StratumConnection> sc(new StratumConnection());
        ABC_CHECK(sc->connect(server));
        bc.reset(sc.release());
    }
    else
    {
        return ABC_ERROR(ABC_CC_Error, "Unknown server type " + server);
    }

    // Height callbacks:
    subscribeHeight(bc.get());

    // Check for mining fees:
    auto sc = dynamic_cast<StratumConnection *>(bc.get());
    if (generalEstimateFeesNeedUpdate() && sc)
    {
        fetchFeeEstimate(1, sc);
        fetchFeeEstimate(2, sc);
        fetchFeeEstimate(3, sc);
        fetchFeeEstimate(4, sc);
        fetchFeeEstimate(5, sc);
        fetchFeeEstimate(6, sc);
        fetchFeeEstimate(7, sc);
    }

    connections_.push_back(bc.release());
    ABC_DebugLog("Connected to %s as %d", server.c_str(), index);

    return Status();
}

IBitcoinConnection *
TxUpdater::pickServer(const std::string &name)
{
    // If the requested server is connected, only consider that:
    for (auto *bc: connections_)
        if (name == bc->uri() && !failedServers_.count(bc->uri()))
            return bc->queueFull() ? nullptr : bc;

    // Otherwise, use any server:
    return pickOtherServer();
}

IBitcoinConnection *
TxUpdater::pickOtherServer(const std::string &name)
{
    IBitcoinConnection *fallback = nullptr;

    for (auto *bc: connections_)
    {
        if (!bc->queueFull() && !failedServers_.count(bc->uri()))
        {
            if (name != bc->uri())
                return bc; // Just what we want!
            else
                fallback = bc; // Not our first choice, but tolerable.
        }
    }

    return fallback;
}

void
TxUpdater::subscribeHeight(IBitcoinConnection *bc)
{
    const auto uri = bc->uri();
    auto onError = [this, uri](Status s)
    {
        ABC_DebugLog("%s: height subscribe failed (%s)",
                     uri.c_str(), s.message().c_str());
        failedServers_.insert(uri);
    };

    unsigned long long queryTime = ServerCache::getCurrentTimeMilliSeconds();

    auto onReply = [this, uri, queryTime](size_t height)
    {
        // Set the response time in the cache
        unsigned long long responseTime = ServerCache::getCurrentTimeMilliSeconds();
        cache_.servers.setResponseTime(uri, responseTime - queryTime);

        ABC_DebugLog("%s: height %d returned %d ms", uri.c_str(), height,
                     responseTime - queryTime);
        size_t oldHeight = cache_.blocks.heightSet(height);

        if (oldHeight > height + 2)
        {
            // This server is behind in block height. Disconnect then penalize it a lot
            cache_.servers.serverScoreDown(uri, 20);
        }
        else if (oldHeight <= height)
        {
            cache_.servers.serverScoreUp(uri); // Point for returning a valid height
            if (oldHeight < height)
            {
                cache_.servers.serverScoreUp(uri); // Point for returning a newer height


                // Update addresses with unconfirmed txs:
                const auto statuses = cache_.txs.statuses(cache_.addresses.txids());
                for (const auto status: statuses)
                {
                    if (!status.second.height)
                    {
                        for (const auto &io: status.first.ios)
                        {
                            ABC_DebugLog("Marking %s dirty (tx height check)",
                                         io.address.c_str());
                            cache_.addresses.updateStratumHash(io.address);
                        }
                    }
                }
            }
        }
    };

    bc->heightSubscribe(onError, onReply);
}

void
TxUpdater::subscribeAddress(const std::string &address, IBitcoinConnection *bc)
{
    // If we are already subscribed, mark the address as up-to-date:
    if (bc->addressSubscribed(address))
    {
        cache_.addresses.updateSubscribe(address);
        return;
    }

    const auto uri = bc->uri();
    auto onError = [this, address, uri](Status s)
    {
        ABC_DebugLog("%s: %s subscribe failed (%s)",
                     uri.c_str(), address.c_str(), s.message().c_str());
        failedServers_.insert(uri);
    };

    auto onReply = [this, address, uri](const std::string &stateHash)
    {
        if (cache_.addresses.updateStratumHash(address, stateHash))
        {
            cache_.servers.serverScoreUp(uri); // Point for returning a new hash
            addressServers_[address] = uri;
            ABC_DebugLog("%s: %s subscribe reply (dirty) %s",
                         uri.c_str(), address.c_str(), stateHash.c_str());
        }
        else
        {
            ABC_DebugLog("%s: %s subscribe reply (clean) %s",
                         uri.c_str(), address.c_str(), stateHash.c_str());
        }
    };

    bc->addressSubscribe(onError, onReply, address);
}

void
TxUpdater::fetchAddress(const std::string &address, IBitcoinConnection *bc)
{
    if (wipAddresses_.count(address))
        return;
    wipAddresses_.insert(address);

    const auto uri = bc->uri();
    auto onError = [this, address, uri](Status s)
    {
        ABC_DebugLog("%s: %s fetch failed (%s)",
                     uri.c_str(), address.c_str(), s.message().c_str());
        failedServers_.insert(uri);
        wipAddresses_.erase(address);
    };

    unsigned long long queryTime = ServerCache::getCurrentTimeMilliSeconds();

    auto onReply = [this, address, uri, queryTime](const AddressHistory &history)
    {
        unsigned long long responseTime = ServerCache::getCurrentTimeMilliSeconds();
        cache_.servers.setResponseTime(uri, responseTime - queryTime);

        ABC_DebugLog("%s: %s fetched %d TXIDs %d ms", uri.c_str(), address.c_str(),
                     history.size(), responseTime - queryTime);
        wipAddresses_.erase(address);
        addressServers_[address] = uri;

        TxidSet txids;
        for (auto &row: history)
        {
            cache_.txs.confirmed(row.first, row.second);
            txids.insert(row.first);
        }

        if (!history.empty())
        {
            cache_.addresses.update(address, txids);
            cache_.servers.serverScoreUp(uri);

        }
        else
        {
            std::string hash = cache_.addresses.getStratumHash(address);
            if (hash.empty())
            {

                cache_.addresses.update(address, txids);
                cache_.servers.serverScoreUp(uri);
            }
            else
            {
                ABC_DebugLog("%s: %s SERVER ERROR EMPTY TXIDs with hash %s", uri.c_str(),
                             address.c_str(), hash.c_str());
                // Do not trust current server. Force a new server.
                failedServers_.insert(uri);
                cache_.servers.serverScoreDown(uri, 20);
            }
        }
    };

    bc->addressHistoryFetch(onError, onReply, address);
}

void
TxUpdater::fetchTx(const std::string &txid, IBitcoinConnection *bc)
{
    if (wipTxids_.count(txid))
        return;
    wipTxids_.insert(txid);

    const auto uri = bc->uri();
    auto onError = [this, txid, uri](Status s)
    {
        ABC_DebugLog("%s: tx %s fetch failed (%s)",
                     uri.c_str(), txid.c_str(), s.message().c_str());
        failedServers_.insert(uri);
        wipTxids_.erase(txid);
    };

    unsigned long long queryTime = ServerCache::getCurrentTimeMilliSeconds();

    auto onReply = [this, txid, uri, queryTime](const bc::transaction_type &tx)
    {
        unsigned long long responseTime = ServerCache::getCurrentTimeMilliSeconds();
        cache_.servers.setResponseTime(uri, responseTime - queryTime);

        ABC_DebugLog("%s: tx %s fetched", uri.c_str(), txid.c_str());
        wipTxids_.erase(txid);

        cache_.txs.insert(tx);
        cache_.addresses.update();
        cacheDirty = true;
        cache_.servers.serverScoreUp(uri);
    };

    ABC_DebugLog("%s: tx %s requested", uri.c_str(), txid.c_str());
    bc->txDataFetch(onError, onReply, txid);
}

void
TxUpdater::fetchFeeEstimate(size_t blocks, StratumConnection *sc)
{
    const auto uri = sc->uri();
    auto onError = [this, blocks, uri](Status s)
    {
        ABC_DebugLog("%s: get fees for %d blocks failed (%s)",
                     uri.c_str(), blocks, s.message().c_str());
    };

    unsigned long long queryTime = ServerCache::getCurrentTimeMilliSeconds();
    auto onReply = [this, blocks, uri, queryTime](double fee)
    {
        unsigned long long responseTime = ServerCache::getCurrentTimeMilliSeconds();
        cache_.servers.setResponseTime(uri, responseTime - queryTime);

        ABC_DebugLog("%s: returned fee %lf for %d blocks %d ms",
                     uri.c_str(), fee, blocks, responseTime - queryTime);
        generalEstimateFeesUpdate(blocks, fee);
    };

    sc->feeEstimateFetch(onError, onReply, blocks);
}

void
TxUpdater::blockHeaderFetch(size_t height, IBitcoinConnection *bc)
{
    const auto uri = bc->uri();
    auto onError = [this, height, uri](Status s)
    {
        ABC_DebugLog("%s: header %d fetch failed (%s)",
                     uri.c_str(), height, s.message().c_str());
        failedServers_.insert(uri);
    };

    unsigned long long queryTime = ServerCache::getCurrentTimeMilliSeconds();
    auto onReply = [this, height, uri,
                          queryTime](const bc::block_header_type &header)
    {
        unsigned long long responseTime = ServerCache::getCurrentTimeMilliSeconds();
        cache_.servers.setResponseTime(uri, responseTime - queryTime);

        ABC_DebugLog("%s: header %d fetched %d ms",
                     uri.c_str(), height, responseTime - queryTime);

        bool didInsert = cache_.blocks.headerInsert(height, header);
        if (didInsert)
            cache_.servers.serverScoreUp(uri);
    };

    bc->blockHeaderFetch(onError, onReply, height);
}

} // namespace abcd
