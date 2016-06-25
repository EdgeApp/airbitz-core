/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_CACHE_ADDRESS_CACHE_HPP
#define ABCD_BITCOIN_CACHE_ADDRESS_CACHE_HPP

#include "../Typedefs.hpp"
#include "../../util/Status.hpp"
#include <time.h>
#include <map>
#include <mutex>

namespace abcd {

class JsonObject;
class TxCache;
struct TxInfo;

/**
 * Status of an address that needs work.
 */
struct AddressStatus
{
    /** The address in question. */
    std::string address;

    /** True if our address state is known to be dirty. */
    bool dirty;

    /** True if this address hasn't been checked in a while. */
    bool needsCheck;

    /** The time of the next check. Used for sorting. */
    time_t nextCheck;

    /** The size of the known transaction list. */
    bool count;

    /** A list of transactions that are missing from the cache. */
    TxidSet missingTxids;
};

/**
 * Sorts statuses by order of urgency.
 */
bool
operator <(const AddressStatus &a, const AddressStatus &b);

/**
 * Tracks address query freshness.
 *
 * The long-term plan is to make this class work with the transaction cache.
 * It should be able to pick good poll frequencies for each address,
 * and should also generate new addresses based on the HD gap limit.
 * This class should also cache its contents on disk,
 * avoiding the need to re-check everything on each login.
 *
 * This will allow the `AddressDb` to be a simple metadata store,
 * with no need to handle Bitcoin-specific knowledge.
 */
class AddressCache
{
public:
    typedef std::function<void ()> Callback;
    typedef std::function<void (const std::string &txid)> TxidCallback;
    typedef std::function<void (const std::string &address)> CompleteCallback;

    // Lifetime ------------------------------------------------------------

    AddressCache(TxCache &txCache);

    /**
     * Clears the cache for debugging purposes.
     */
    void
    clear();

    /**
     * Reads the database contents from the provided cache JSON object.
     */
    Status
    load(JsonObject &json);

    /**
     * Saves the database contents to the provided cache JSON object.
     */
    Status
    save(JsonObject &json);

    // Queries -------------------------------------------------------------

    /**
     * Returns the number of completed addresses & total addresses.
     */
    std::pair<size_t, size_t>
    progress() const;

    /**
     * Returns the status of all unsynced addresses.
     * @param sleep If there is no work to be performed,
     * the number of seconds until the next time work will be available.
     */
    std::list<AddressStatus>
    statuses(time_t &sleep) const;

    /**
     * Builds a list of transactions that are relevant to these addresses.
     */
    TxidSet
    txids() const;

    // Updates -------------------------------------------------------------

    /**
     * Begins watching an address.
     */
    void
    insert(const std::string &address, bool sweep=false);

    /**
     * Begins checking the provided address at high speed.
     * Pass a blank address to cancel the priority polling.
     */
    void
    prioritize(const std::string &address);

    /**
     * Indicates that the transaction cache has been updated.
     */
    void
    update();

    /**
     * Updates an address with a new list of relevant transactions.
     */
    void
    update(const std::string &address, const TxidSet &txids);

    /**
     * Updates all addresses touched by a spend.
     */
    void
    updateSpend(TxInfo &info);

    /**
     * Indicates that an address has been subscribed to,
     * so it's not really outdated.
     */
    void
    updateSubscribe(const std::string &address);

    /**
     * Updates the state hash stored with the address.
     * Returns true if the new hash differs from the old hash.
     */
    bool
    updateStratumHash(const std::string &address, const std::string &hash="");

    /**
     * Sets up a callback to notify when addresses change.
     * This wakes up the updater to check for new work.
     */
    void
    wakeupCallbackSet(const Callback &callback);

    /**
     * Provides a callback to be notified when new transactions are complete.
     */
    void
    onTxSet(const TxidCallback &onTx);

    /**
     * Provides a callback to be notified when an address is complete.
     */
    void
    onCompleteSet(const CompleteCallback &onComplete);

private:
    mutable std::recursive_mutex mutex_; // The callbacks force this on us
    TxCache &txCache_;
    std::string priorityAddress_;

    struct AddressRow
    {
        // Persistent state:
        TxidSet txids;
        time_t lastCheck = 0;
        std::string stratumHash;

        // Dynamic state:
        bool dirty = true;
        bool checkedOnce = false;
        bool complete = false; // True if all txids are known to the GUI.
        bool knownComplete = false; // True if `onComplete` has been called.
        bool sweep = false; // True if we don't own this address

        void
        insertTxid(const std::string &txid)
        {
            txids.insert(txid);
            complete = false;
            knownComplete = false;
        }
    };
    std::map<std::string, AddressRow> rows_;

    /**
     * Transactions that are relevant, in the cache,
     * and that the GUI knows about.
     */
    TxidSet knownTxids_;

    Callback wakeupCallback_;
    TxidCallback onTx_;
    CompleteCallback onComplete_;

    time_t
    nextCheck(const std::string &address, const AddressRow &row) const;

    AddressStatus
    status(const std::string &address, const AddressRow &row,
           time_t now) const;

    void
    updateInternal();
};

} // namespace abcd

#endif
