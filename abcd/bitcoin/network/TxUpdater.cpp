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

namespace abcd {

constexpr auto LIBBITCOIN_PREFIX = "tcp://";
constexpr auto STRATUM_PREFIX = "stratum://";
constexpr auto LIBBITCOIN_PREFIX_LENGTH = 6;
constexpr auto STRATUM_PREFIX_LENGTH = 10;

constexpr auto NUM_CONNECT_SERVERS = 4;
constexpr auto MINIMUM_LIBBITCOIN_SERVERS = 0;
constexpr auto MINIMUM_STRATUM_SERVERS = 2;

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

    // This happens once, and never changes:
    if (serverList_.empty())
        serverList_ = generalBitcoinServers();

    for (int i = 0; i < serverList_.size(); i++)
    {
        ABC_DebugLevel(1, "serverList_[%d]=%s", i, serverList_[i].c_str());
    }

    // If we are out of fresh libbitcoin servers, reload the list:
    if (untriedLibbitcoin_.empty())
    {
        for (size_t i = 0; i < serverList_.size(); ++i)
        {
            const auto &server = serverList_[i];
            if (0 == server.compare(0, LIBBITCOIN_PREFIX_LENGTH, LIBBITCOIN_PREFIX))
                untriedLibbitcoin_.insert(i);
        }
    }

    // If we are out of fresh stratum servers, reload the list:
    if (untriedStratum_.empty())
    {
        for (size_t i = 0; i < serverList_.size(); ++i)
        {
            const auto &server = serverList_[i];
            if (0 == server.compare(0, STRATUM_PREFIX_LENGTH, STRATUM_PREFIX))
                untriedStratum_.insert(i);
        }
    }

    // XXX disable Libbitcoin for now until we get better reliability
    untriedLibbitcoin_.clear();

    ABC_DebugLevel(2,"%d libbitcoin untried, %d stratrum untried",
                   untriedLibbitcoin_.size(), untriedStratum_.size());

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
            && (untriedLibbitcoin_.size() || untriedStratum_.size()))
    {
        auto *untriedPrimary = &untriedStratum_;
        auto *primaryCount = &stratumCount;
        auto *untriedSecondary = &untriedLibbitcoin_;
        auto *secondaryCount = &libbitcoinCount;
        long minPrimary = MINIMUM_STRATUM_SERVERS;
        long minSecondary = MINIMUM_LIBBITCOIN_SERVERS;

        if (numConnections % 2 == 1)
        {
            untriedPrimary = &untriedLibbitcoin_;
            untriedSecondary = &untriedStratum_;
            primaryCount = &libbitcoinCount;
            secondaryCount = &stratumCount;
            minPrimary = MINIMUM_LIBBITCOIN_SERVERS;
            minSecondary = MINIMUM_STRATUM_SERVERS;
        }

        if (untriedPrimary->size() &&
                ((minSecondary - *secondaryCount < NUM_CONNECT_SERVERS - connections_.size()) ||
                 (rand() & 8)))
        {
            auto i = untriedPrimary->begin();
            std::advance(i, rand() % untriedPrimary->size());
            if (connectTo(*i).log())
            {
                (*primaryCount)++;
                ++numConnections;
            }
        }
        else if (untriedSecondary->size() &&
                 ((minPrimary - *primaryCount < NUM_CONNECT_SERVERS - connections_.size()) ||
                  (rand() & 8)))
        {
            auto i = untriedSecondary->begin();
            std::advance(i, rand() % untriedSecondary->size());
            if (connectTo(*i).log())
            {
                (*secondaryCount)++;
                ++numConnections;
            }
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
                nextWakeup = bc::client::min_sleep(nextWakeup, sleep);
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
TxUpdater::connectTo(long index)
{
    std::string server = serverList_[index];
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
    if (0 == server.compare(0, LIBBITCOIN_PREFIX_LENGTH, LIBBITCOIN_PREFIX))
    {
        // Libbitcoin server:
        untriedLibbitcoin_.erase(index);
        std::unique_ptr<LibbitcoinConnection> lc(new LibbitcoinConnection(ctx_));
        ABC_CHECK(lc->connect(server, key));
        bc.reset(lc.release());
    }
    else if (0 == server.compare(0, STRATUM_PREFIX_LENGTH, STRATUM_PREFIX))
    {
        // Stratum server:
        untriedStratum_.erase(index);
        std::unique_ptr<StratumConnection> sc(new StratumConnection());
        ABC_CHECK(sc->connect(server));
        bc.reset(sc.release());
    }
    else
    {
        return ABC_ERROR(ABC_CC_Error, "Unknown server type " + server);
    }

    // Height callbacks:
    const auto uri = bc->uri();
    auto onError = [this, uri](Status s)
    {
        ABC_DebugLog("%s: height subscribe failed (%s)",
                     uri.c_str(), s.message().c_str());
        failedServers_.insert(uri);
    };
    auto onReply = [this, uri](size_t height)
    {
        ABC_DebugLog("%s: height %d returned", uri.c_str(), height);
        cache_.blocks.heightSet(height);
    };
    bc->heightSubscribe(onError, onReply);

    // Check for mining fees:
    auto sc = dynamic_cast<StratumConnection *>(bc.get());
    if (generalEstimateFeesNeedUpdate() && sc)
    {
        fetchFeeEstimate(1, sc);
        fetchFeeEstimate(2, sc);
        fetchFeeEstimate(3, sc);
        fetchFeeEstimate(4, sc);
        fetchFeeEstimate(5, sc);
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
            addressServers_[address] = uri;
            ABC_DebugLog("%s: %s subscribe reply (dirty)",
                         uri.c_str(), address.c_str());
        }
        else
        {
            ABC_DebugLog("%s: %s subscribe reply (clean)",
                         uri.c_str(), address.c_str());
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

    auto onReply = [this, address, uri](const AddressHistory &history)
    {
        ABC_DebugLog("%s: %s fetched", uri.c_str(), address.c_str());
        wipAddresses_.erase(address);
        addressServers_[address] = uri;

        TxidSet txids;
        for (auto &row: history)
        {
            cache_.txs.confirmed(row.first, row.second);
            txids.insert(row.first);
        }
        cache_.addresses.update(address, txids);
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

    auto onReply = [this, txid, uri](const bc::transaction_type &tx)
    {
        ABC_DebugLog("**************************************************************");
        ABC_DebugLog("%s: tx %s fetched", uri.c_str(), txid.c_str());
        ABC_DebugLog("**************************************************************");
        wipTxids_.erase(txid);

        cache_.txs.insert(tx);
        cache_.addresses.update();
        cacheDirty = true;
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

    auto onReply = [this, blocks, uri](double fee)
    {
        ABC_DebugLog("%s: returned fee %lf for %d blocks",
                     uri.c_str(), fee, blocks);

        if (fee > 0)
        {
            generalEstimateFeesUpdate(blocks, fee);
        }
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

    auto onReply = [this, height, uri](const bc::block_header_type &header)
    {
        ABC_DebugLog("%s: header %d fetched",
                     uri.c_str(), height);

        cache_.blocks.headerInsert(height, header);
    };

    bc->blockHeaderFetch(onError, onReply, height);
}

} // namespace abcd
