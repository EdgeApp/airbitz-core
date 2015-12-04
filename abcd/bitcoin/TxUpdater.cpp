/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */

#include "TxUpdater.hpp"
#include "../General.hpp"
#include "../util/Debug.hpp"
#include <list>

namespace abcd {

#define LIBBITCOIN_PREFIX           "tcp://"
#define STRATUM_PREFIX              "stratum://"
#define LIBBITCOIN_PREFIX_LENGTH    6
#define STRATUM_PREFIX_LENGTH       10

#define ALL_SERVERS                 -1
#define NO_SERVERS                  -9999
#define NUM_CONNECT_SERVERS         4
#define MINIMUM_LIBBITCOIN_SERVERS  1
#define MINIMUM_STRATUM_SERVERS     1

constexpr unsigned max_queries = 10;

/**
 * An address that needs to be checked.
 * More outdated addresses sort earlier in the list.
 */
struct ToCheck
{
    bc::client::sleep_time oldness;
    bc::payment_address address;

    bool
    operator<(const ToCheck &b) const
    {
        return oldness > b.oldness;
    }
};

using std::placeholders::_1;

TxUpdater::~TxUpdater()
{
    disconnect();
}

TxUpdater::TxUpdater(TxDatabase &db, void *ctx, TxCallbacks &callbacks):
    db_(db),
    ctx_(ctx),
    callbacks_(callbacks),
    failed_(false),
    last_wakeup_(std::chrono::steady_clock::now())
{
}

void
TxUpdater::disconnect()
{
    wantConnection = false;

    for (auto &i: connections_)
        delete i;
    connections_.clear();

    ABC_DebugLog("Disconnected from all servers.");
}

Status
TxUpdater::connect()
{
    wantConnection = true;

    // This happens once, and never changes:
    if (serverList_.empty())
        serverList_ = generalBitcoinServers();

    // If we have full connections then wipe them out and start over.
    // This was likely due to a refresh
    if (NUM_CONNECT_SERVERS <= connections_.size())
    {
        disconnect();
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

    ABC_DebugLevel(2,"%d libbitcoin untried, %d stratrum untried",
                   untriedLibbitcoin_.size(), untriedStratum_.size());

    // Count the number of existing connections:
    auto stratumCount = std::count_if(connections_.begin(), connections_.end(),
    [](Connection *c) { return c->type == ConnectionType::stratum; });
    auto libbitcoinCount = std::count_if(connections_.begin(), connections_.end(),
    [](Connection *c) { return c->type == ConnectionType::libbitcoin; });

    // Let's make some connections:
    srand(time(nullptr));
    while (connections_.size() < NUM_CONNECT_SERVERS &&
            (untriedLibbitcoin_.size() || untriedStratum_.size()))
    {
        // Connect to a stratum server,
        // but only if we have enough libbitcoin servers,
        // and we either need a stratum server or we get lucky:
        if (untriedStratum_.size() &&
                MINIMUM_LIBBITCOIN_SERVERS <= libbitcoinCount &&
                (stratumCount < MINIMUM_STRATUM_SERVERS || (rand() & 8)))
        {
            auto i = untriedStratum_.begin();
            std::advance(i, rand() % untriedStratum_.size());
            if (connectTo(*i).log())
                ++stratumCount;
        }
        else if (untriedLibbitcoin_.size())
        {
            auto i = untriedLibbitcoin_.begin();
            std::advance(i, rand() % untriedLibbitcoin_.size());
            if (connectTo(*i).log())
                ++libbitcoinCount;
        }
    }

    if (connections_.size())
    {
        // Check for new blocks:
        get_height();

        // Handle block-fork checks & unconfirmed transactions:
        db_.foreach_unconfirmed(std::bind(&TxUpdater::get_index, this, _1,
                                          ALL_SERVERS));
        queue_get_indices(ALL_SERVERS);

        // Transmit all unsent transactions:
        db_.foreach_unsent(std::bind(&TxUpdater::send_tx, this, _1));
    }

    return Status();
}

void TxUpdater::watch(const bc::payment_address &address,
                      bc::client::sleep_time poll)
{
    ABC_DebugLevel(2,"watch() address=%s",address.encoded().c_str());
    // Only insert if it isn't already present:
    rows_[address] = AddressRow{poll, std::chrono::steady_clock::now() - poll};
    query_address(address, ALL_SERVERS);

}

void TxUpdater::send(bc::transaction_type tx)
{
    send_tx(tx);
}

AddressSet TxUpdater::watching()
{
    AddressSet out;
    for (auto &row: rows_)
        out.insert(row.first.encoded());
    return out;
}

bc::client::sleep_time TxUpdater::wakeup()
{
    bc::client::sleep_time next_wakeup(0);
    auto now = std::chrono::steady_clock::now();

    // Figure out when our next block check is:
    auto period = std::chrono::seconds(30);
    auto elapsed = std::chrono::duration_cast<bc::client::sleep_time>(
                       now - last_wakeup_);
    if (period <= elapsed)
    {
        get_height();
        last_wakeup_ = now;
        elapsed = bc::client::sleep_time::zero();
    }
    next_wakeup = period - elapsed;

    // Build a list of all the addresses that are due for a checkup:
    std::list<ToCheck> toCheck;
    for (auto &row: rows_)
    {
        auto poll_time = row.second.poll_time;
        auto elapsed = std::chrono::duration_cast<bc::client::sleep_time>(
                           now - row.second.last_check);
        if (poll_time <= elapsed)
            toCheck.push_back(ToCheck{elapsed - poll_time, row.first});
        else
            next_wakeup = bc::client::min_sleep(next_wakeup, poll_time - elapsed);
    }

    // Process the most outdated addresses first:
    toCheck.sort();
    for (const auto &i: toCheck)
    {
        for (const auto c: connections_)
        {
            auto &row = rows_[i.address];
            if (c->queued_queries_ < max_queries ||
                    row.poll_time < std::chrono::seconds(6))
            {
                ABC_DebugLevel(2,"wakeup() idx=%d Calling query_address %s",
                               c->server_index,
                               i.address.encoded().c_str());
                next_wakeup = bc::client::min_sleep(next_wakeup, row.poll_time);
                query_address(i.address, c->server_index);
            }
        }
    }

    // Update the sockets:
    for (auto &connection: connections_)
    {
        if (ConnectionType::libbitcoin == connection->type)
        {
            connection->bc_socket.forward(connection->bc_codec);
            next_wakeup = bc::client::min_sleep(next_wakeup,
                                                connection->bc_codec.wakeup());
        }
        else if (ConnectionType::stratum == connection->type)
        {
            SleepTime sleep;
            if (!connection->stratumCodec.wakeup(sleep).log())
            {
                failed_server_idx_ = connection->server_index;
                failed_ = true;
            }
            next_wakeup = bc::client::min_sleep(next_wakeup, sleep);
        }
    }

    // Handle the last server failure:
    if (failed_)
    {
        auto p = [this](Connection *c) { return failed_server_idx_ == c->server_index; };
        auto i = std::find_if(connections_.begin(), connections_.end(), p);
        if (connections_.end() != i)
        {
            delete *i;
            connections_.erase(i);
            ABC_DebugLog("Disconnected from %d (%s)", failed_server_idx_,
                         serverList_[failed_server_idx_].c_str());
        }
        failed_server_idx_ = NO_SERVERS;
        failed_ = false;
    }

    // Connect to more servers:
    if (wantConnection && connections_.size() < NUM_CONNECT_SERVERS)
        connect().log();

    return next_wakeup;
}

std::vector<zmq_pollitem_t>
TxUpdater::pollitems()
{
    std::vector<zmq_pollitem_t> out;
    for (const auto &connection: connections_)
    {
        if (ConnectionType::libbitcoin == connection->type)
        {
            out.push_back(connection->bc_socket.pollitem());
        }
        else if (ConnectionType::stratum == connection->type)
        {
            zmq_pollitem_t pollitem =
            {
                nullptr, connection->stratumCodec.pollfd(), ZMQ_POLLIN, ZMQ_POLLOUT
            };
            out.push_back(pollitem);
        }
    }
    return out;
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
    std::unique_ptr<Connection> bconn(new Connection(ctx_, index));
    if (0 == server.compare(0, LIBBITCOIN_PREFIX_LENGTH, LIBBITCOIN_PREFIX))
    {
        // Libbitcoin server:
        untriedLibbitcoin_.erase(index);
        bconn->type = ConnectionType::libbitcoin;
        if(!bconn->bc_socket.connect(server, key))
            return ABC_ERROR(ABC_CC_Error, "Could not connect to " + server);
    }
    else if (0 == server.compare(0, STRATUM_PREFIX_LENGTH, STRATUM_PREFIX))
    {
        // Stratum server:
        untriedStratum_.erase(index);
        bconn->type = ConnectionType::stratum;

        // Extract the server name and port:
        auto last = server.find(":", STRATUM_PREFIX_LENGTH);
        std::string serverName = server.substr(STRATUM_PREFIX_LENGTH,
                                               last - STRATUM_PREFIX_LENGTH);
        std::string serverPort = server.substr(last + 1, std::string::npos);
        int port = atoi(serverPort.c_str());

        ABC_CHECK(bconn->stratumCodec.connect(serverName, port));
    }
    else
    {
        return ABC_ERROR(ABC_CC_Error, "Unknown server type " + server);
    }

    connections_.push_back(bconn.release());
    ABC_DebugLog("Connected to %s as %d", server.c_str(), index);

    return Status();
}

void TxUpdater::watch_tx(bc::hash_digest txid, bool want_inputs, int idx)
{
    db_.reset_timestamp(txid);
    std::string str = bc::encode_hash(txid);
    if (!db_.txidExists(txid))
    {

        ABC_DebugLevel(1,
                       "*************************************************************");
        ABC_DebugLevel(1,"*** watch_tx idx=%d FOUND NEW TRANSACTION %s ****", idx,
                       str.c_str());
        ABC_DebugLevel(1,
                       "*************************************************************");
        get_tx(txid, want_inputs, idx);
    }
    else
    {
        ABC_DebugLevel(2,"*** watch_tx idx=%d TRANSACTION %s already in DB ****", idx,
                       str.c_str());
        if (want_inputs)
        {
            ABC_DebugLevel(2,"*** watch_tx idx=%d getting inputs for tx=%s ****", idx,
                           str.c_str());
            get_inputs(db_.txidLookup(txid), idx);
        }
    }
}

void TxUpdater::get_inputs(const bc::transaction_type &tx, int idx)
{
    for (auto &input: tx.inputs)
        watch_tx(input.previous_output.hash, false, idx);
}

void TxUpdater::query_done(int idx, Connection &bconn)
{
    bconn.queued_queries_--;

    if (bconn.queued_queries_ < 0)
    {
        ABC_DebugLevel(1,"query_done idx=%d queued_queries=%d GOING NEGATIVE!!", idx,
                       bconn.queued_queries_);
    }
    else if (bconn.queued_queries_ == 0)
    {
        ABC_DebugLevel(1,"query_done idx=%d queued_queries=%d CLEARED QUEUE", idx,
                       bconn.queued_queries_);
    }
    else if (bconn.queued_queries_ + 1 >= max_queries)
    {
        ABC_DebugLevel(2,"query_done idx=%d queued_queries=%d NEAR MAX_QUERIES", idx,
                       bconn.queued_queries_);
    }

    // Iterate over all the connections. If empty, fire off the callback.
    int total_queries = 0;
    for (auto &it: connections_)
    {
        total_queries += it->queued_queries_;
    }

    if (!total_queries)
        callbacks_.on_quiet();
}

void TxUpdater::queue_get_indices(int idx)
{
    int total_queued_indices = 0;
    for (auto &it: connections_)
    {
        total_queued_indices += it->queued_get_indices_;
    }

    if (total_queued_indices)
        return;

    db_.foreach_forked(std::bind(&TxUpdater::get_index, this, _1, idx));
}

// - server queries --------------------

void TxUpdater::get_height()
{
    if (!connections_.size())
        return;

    for (auto &it: connections_)
    {
        // TODO: support get_height for Stratum
        if (ConnectionType::stratum == it->type)
            continue;

        Connection &bconn = *it;
        auto idx = bconn.server_index;

        auto on_error = [this, idx, &bconn](const std::error_code &error)
        {
            if (!failed_) ABC_DebugLevel(1, "get_height server idx=%d failed: %s", idx,
                                             error.message().c_str());
            failed_ = true;
            failed_server_idx_ = idx;
            bconn.queued_get_height_--;
            ABC_DebugLevel(1, "get_height on_error queued_get_height=%d",
                           bconn.queued_get_height_);
        };

        auto on_done = [this, idx, &bconn](size_t height)
        {
            if (db_.last_height() < height)
            {
                db_.at_height(height);
                callbacks_.on_height(height);

                // Query all unconfirmed transactions:
                db_.foreach_unconfirmed(std::bind(&TxUpdater::get_index, this, _1, idx));
                queue_get_indices(idx);
                ABC_DebugLevel(2, "get_height server idx=%d height=%d", idx, height);
            }
            bconn.queued_get_height_--;
            ABC_DebugLevel(2, "get_height on_done queued_get_height=%d",
                           bconn.queued_get_height_);
        };

        bconn.queued_get_height_++;
        ABC_DebugLevel(2, "get_height queued_get_height=%d", bconn.queued_get_height_);
        bconn.bc_codec.fetch_last_height(on_error, on_done);

        // Only use the first server response.
        break;
    }
}

void TxUpdater::get_tx(bc::hash_digest txid, bool want_inputs, int server_index)
{
    std::string str = bc::encode_hash(txid);

    for (auto it: connections_)
    {
        Connection &bconn = *it;

        // If there is a preferred server index to use. Only query that server
        if (ALL_SERVERS != server_index)
        {
            if (bconn.server_index != server_index)
                continue;
        }

        auto idx = bconn.server_index;

        auto on_error = [this, txid, str, want_inputs, &bconn,
                         idx](const std::error_code &error)
        {
            // A failure means the transaction might be in the mempool:
            (void)error;
            ABC_DebugLevel(2,"get_tx ON_ERROR no idx=%d txid=%s calling get_tx_mem", idx,
                           str.c_str());
            get_tx_mem(txid, want_inputs, idx);
            query_done(idx, bconn);
        };

        auto on_done = [this, txid, str, want_inputs, &bconn,
                        idx](const bc::transaction_type &tx)
        {
            ABC_DebugLevel(2,"get_tx ENTER ON_DONE idx=%d txid=%s", idx, str.c_str());
            BITCOIN_ASSERT(txid == bc::hash_transaction(tx));
            if (db_.insert(tx, TxState::unconfirmed))
                callbacks_.on_add(tx);
            if (want_inputs)
            {
                ABC_DebugLevel(2,"get_tx idx=%d found txid=%s calling get_inputs", idx,
                               str.c_str());
                get_inputs(tx, idx);
            }
            ABC_DebugLevel(2,"get_tx idx=%d found txid=%s calling get_index", idx,
                           str.c_str());
            get_index(txid, idx);
            query_done(idx, bconn);
            ABC_DebugLevel(2,"get_tx EXIT ON_DONE idx=%d txid=%s", idx, str.c_str());
        };

        bconn.queued_queries_++;
        ABC_DebugLevel(2,"get_tx idx=%d queued_queries=%d", idx, bconn.queued_queries_);

        if (ConnectionType::libbitcoin == bconn.type)
            bconn.bc_codec.fetch_transaction(on_error, on_done, txid);
        else if (ConnectionType::stratum == bconn.type)
            bconn.stratumCodec.getTx(on_error, on_done, txid);
    }
}

void TxUpdater::get_tx_mem(bc::hash_digest txid, bool want_inputs,
                           int server_index)
{
    std::string str = bc::encode_hash(txid);

    for (auto &it: connections_)
    {
        Connection &bconn = *it;

        // If there is a preferred server index to use. Only query that server
        if (ALL_SERVERS != server_index)
        {
            if (bconn.server_index != server_index)
                continue;
        }

        auto idx = bconn.server_index;

        auto on_error = [this, idx, str, &bconn](const std::error_code &error)
        {
            ABC_DebugLevel(1,"get_tx_mem ON_ERROR no idx=%d txid=%s NOT IN MEMPOOL", idx,
                           str.c_str());

            failed_ = true;
            failed_server_idx_ = idx;
            query_done(idx, bconn);
        };

        auto on_done = [this, txid, str, want_inputs, idx,
                        &bconn](const bc::transaction_type &tx)
        {
            ABC_DebugLevel(2,"get_tx_mem ENTER ON_DONE idx=%d txid=%s FOUND IN MEMPOOL",
                           idx, str.c_str());
            BITCOIN_ASSERT(txid == bc::hash_transaction(tx));
            if (db_.insert(tx, TxState::unconfirmed))
                callbacks_.on_add(tx);
            if (want_inputs)
            {
                ABC_DebugLevel(2,"get_tx_mem ON_DONE calling get_inputs idx=%d txid=%s", idx,
                               str.c_str());
                get_inputs(tx, idx);
            }
            ABC_DebugLevel(2,"get_tx_mem ON_DONE calling get_index idx=%d txid=%s", idx,
                           str.c_str());
            get_index(txid, idx);
            query_done(idx, bconn);
            ABC_DebugLevel(2,"get_tx_mem EXIT ON_DONE idx=%d txid=%s", idx, str.c_str());
        };

        bconn.queued_queries_++;
        if (ConnectionType::libbitcoin == bconn.type)
            bconn.bc_codec.fetch_unconfirmed_transaction(on_error, on_done, txid);
        else if (ConnectionType::stratum == bconn.type)
            bconn.stratumCodec.getTx(on_error, on_done, txid);
    }
}

void TxUpdater::get_index(bc::hash_digest txid, int server_index)
{

    for (auto &it: connections_)
    {
        // TODO: support get_index for Stratum
        if (ConnectionType::stratum == it->type)
            continue;

        Connection &bconn = *it;

        // TODO: Removing the code below might cause unnecessary server load. Since Stratum can't query
        // txid height using just a txid, we have to rely on Libbitcoin to do this.

//        // If there is a preferred server index to use. Only query that server
//        if (ALL_SERVERS != server_index)
//        {
//            if (bconn.server_index != server_index)
//                continue;
//        }
//
        auto idx = bconn.server_index;
        auto on_error = [this, txid, idx, &bconn](const std::error_code &error)
        {
            // A failure means that the transaction is unconfirmed:
            (void)error;
            db_.unconfirmed(txid);

            bconn.queued_get_indices_--;
            queue_get_indices(idx);
        };

        auto on_done = [this, txid, idx, &bconn](size_t block_height, size_t index)
        {
            // The transaction is confirmed:
            (void)index;

            db_.confirmed(txid, block_height);

            bconn.queued_get_indices_--;
            queue_get_indices(idx);
            ABC_DebugLevel(2,"get_index SUCCESS server idx: %d", idx);
        };

        bconn.queued_get_indices_++;
        bconn.bc_codec.fetch_transaction_index(on_error, on_done, txid);
    }
}

void TxUpdater::send_tx(const bc::transaction_type &tx)
{
    for (auto &it: connections_)
    {
        // TODO: support send_tx for Stratum
        if (ConnectionType::stratum == it->type)
            continue;

        auto on_error = [](const std::error_code &error) {};

        auto on_done = [this, tx]()
        {
            db_.unconfirmed(bc::hash_transaction(tx));
        };

        it->bc_codec.broadcast_transaction(on_error, on_done, tx);
    }
}

void TxUpdater::query_address(const bc::payment_address &address,
                              int server_index)
{
    ABC_DebugLevel(2,"query_address ENTER %s", address.encoded().c_str());
    rows_[address].last_check = std::chrono::steady_clock::now();
    std::string servers = "";
    std::string maxed_servers = "";
    int total_queries = 0;
    int num_servers = 0;
    int num_maxed_servers = 0;

    if (!connections_.size())
    {
        ABC_DebugLevel(2,"query_address connections_ vector empty");
    }

    for (auto &it: connections_)
    {
        Connection &bconn = *it;
        auto idx = bconn.server_index;

        // If there is a preferred server index to use. Only query that server
        if (ALL_SERVERS != server_index)
        {
            if (bconn.server_index != server_index)
                continue;
        }

        if (bconn.queued_queries_ > max_queries)
        {
            if (num_maxed_servers)
                maxed_servers += " ";
            maxed_servers += std::to_string(idx);
            num_maxed_servers++;
            ABC_DebugLevel(2,
                           "TxUpdater::query_address() idx=%d (queued > max) for address=%s queued_queries=%d",
                           idx, address.encoded().c_str(), bconn.queued_queries_);
            continue;
        }

        if (num_servers)
            servers += " ";
        servers += std::to_string(idx);

        auto on_error = [this, idx, address, &bconn](const std::error_code &error)
        {
            ABC_DebugLevel(1,"query_address ON_ERROR idx:%d addr:%s failed:%s",
                           idx, address.encoded().c_str(), error.message().c_str());
            failed_ = true;
            failed_server_idx_ = idx;
            query_done(idx, bconn);
        };

        auto on_done = [this, idx, address,
                        &bconn](const bc::client::history_list &history)
        {
            ABC_DebugLevel(2,"TxUpdater::query_address ENTER ON_DONE idx:%d addr:%s", idx,
                           address.encoded().c_str());
            ABC_DebugLevel(2,"   Looping over address transactions... ");
            for (auto &row: history)
            {
                ABC_DebugLevel(2,"   Watching output tx=%s",
                               bc::encode_hash(row.output.hash).c_str());
                watch_tx(row.output.hash, true, idx);
                if (row.spend.hash != bc::null_hash)
                {
                    watch_tx(row.spend.hash, true, idx);
                    ABC_DebugLevel(2,"   Watching spend tx=%s",
                                   bc::encode_hash(row.spend.hash).c_str());
                }
            }
            query_done(idx, bconn);
            ABC_DebugLevel(2,"TxUpdater::query_address EXIT ON_DONE idx:%d addr:%s", idx,
                           address.encoded().c_str());
        };

        bconn.queued_queries_++;
        num_servers++;
        total_queries += bconn.queued_queries_;
        ABC_DebugLevel(2,"TxUpdater::query_address idx=%d queued_queries=%d %s", idx,
                       bconn.queued_queries_, address.encoded().c_str());

        if (ConnectionType::libbitcoin == bconn.type)
            bconn.bc_codec.address_fetch_history(on_error, on_done, address);
        else if (ConnectionType::stratum == bconn.type)
            bconn.stratumCodec.getAddressHistory(on_error, on_done, address);
    }

    if (num_servers)
        ABC_DebugLevel(2,"query_address svrs=[%s] maxed_svrs=[%s] avg_q=%.1f addr=%s",
                       servers.c_str(), maxed_servers.c_str(), (float)total_queries/(float)num_servers,
                       address.encoded().c_str());

    ABC_DebugLevel(2,"query_address EXIT %s", address.encoded().c_str());

}

static void on_unknown_nop(const std::string &)
{
}

TxUpdater::Connection::Connection(void *ctx, long server_index):
    bc_socket(ctx),
    bc_codec(bc_socket, on_unknown_nop, std::chrono::seconds(10), 0),
    queued_queries_(0),
    queued_get_indices_(0),
    queued_get_height_(0),
    server_index(server_index)
{
}

} // namespace abcd
