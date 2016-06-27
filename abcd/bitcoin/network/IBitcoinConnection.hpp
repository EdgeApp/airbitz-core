/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_NETWORK_I_BITCOIN_CONNECTION_HPP
#define ABCD_BITCOIN_NETWORK_I_BITCOIN_CONNECTION_HPP

#include "../Typedefs.hpp"
#include "../../util/Data.hpp"
#include <map>

namespace abcd {

/**
 * Map from txids to block heights.
 */
typedef std::map<std::string, size_t> AddressHistory;

typedef std::function<void (unsigned height)> HeightCallback;
typedef std::function<void (const AddressHistory &history)> AddressCallback;
typedef std::function<void (const std::string &stateHash)>
AddressUpdateCallback;
typedef std::function<void (const libbitcoin::transaction_type &tx)> TxCallback;
typedef std::function<void (const libbitcoin::block_header_type &header)>
HeaderCallback;

/**
 * A connection to the Bitcoin network.
 * This combines the common features from both libbitcoin and Stratum.
 */
class IBitcoinConnection
{
public:
    virtual ~IBitcoinConnection() {}

    /**
     * Returns the server name for this connection.
     */
    virtual std::string
    uri() = 0;

    /**
     * Returns true if the connection is saturated with outstanding requests.
     */
    virtual bool
    queueFull() = 0;

    /**
     * Begins watching for blockchain height changes.
     */
    virtual void
    heightSubscribe(const StatusCallback &onError,
                    const HeightCallback &onReply) = 0;

    /**
     * Begins watching for address history changes.
     * @param onReply called any time a change happens to this address.
     */
    virtual void
    addressSubscribe(const StatusCallback &onError,
                     const AddressUpdateCallback &onReply,
                     const std::string &address) = 0;

    /**
     * Returns true if the connection is subscribed to this address.
     */
    virtual bool
    addressSubscribed(const std::string &address) = 0;

    /**
     * Fetches the transaction history for an address.
     */
    virtual void
    addressHistoryFetch(const StatusCallback &onError,
                        const AddressCallback &onReply,
                        const std::string &address) = 0;

    /**
     * Fetches the raw contents of a transaction.
     */
    virtual void
    txDataFetch(const StatusCallback &onError,
                const TxCallback &onReply,
                const std::string &txid) = 0;

    /**
     * Fetches the header for a block at a particular height.
     */
    virtual void
    blockHeaderFetch(const StatusCallback &onError,
                     const HeaderCallback &onReply,
                     size_t height) = 0;
};

} // namespace abcd

#endif
