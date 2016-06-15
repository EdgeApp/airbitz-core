/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "AddressCache.hpp"
#include "TxCache.hpp"
#include "../../json/JsonArray.hpp"
#include "../../json/JsonObject.hpp"

namespace abcd {

constexpr auto periodDefault = 20;
constexpr auto periodPriority = 4;

struct CacheJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(CacheJson, JsonObject)

    ABC_JSON_VALUE(addresses, "addresses", JsonArray)
};

struct AddressJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(AddressJson, JsonObject)

    ABC_JSON_STRING(address, "address", 0)
    ABC_JSON_VALUE(txids, "txids", JsonArray)
    ABC_JSON_INTEGER(lastCheck, "lastCheck", 0)
    ABC_JSON_STRING(stateHash, "stratumHash", 0)
};

bool
operator <(const AddressStatus &a, const AddressStatus &b)
{
    // A longer missing transaction list is more urgent (sorts lower):
    if (a.missingTxids.size() != b.missingTxids.size())
        return a.missingTxids.size() > b.missingTxids.size();

    // Empty addresses and heavily-used addresses sort first:
    if (a.count != b.count)
        return !a.count || a.count > b.count;

    // Earlier times are more urgent:
    return a.nextCheck < b.nextCheck;
}

AddressCache::AddressCache(TxCache &txCache):
    txCache_(txCache)
{
}

void
AddressCache::clear()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    priorityAddress_ = "";
    for (auto &row: rows_)
        row.second = AddressRow();
    knownTxids_.clear();
}

Status
AddressCache::load(JsonObject &json)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    CacheJson cacheJson(json);
    const auto now = time(nullptr);

    auto arrayJson = cacheJson.addresses();
    size_t size = arrayJson.size();
    for (size_t i = 0; i < size; i++)
    {
        AddressJson addressJson(arrayJson[i]);
        if (addressJson.addressOk())
        {
            auto address = addressJson.address();
            AddressRow row;

            auto arrayJson = addressJson.txids();
            size_t size = arrayJson.size();
            for (size_t i = 0; i < size; i++)
            {
                auto stringJson = arrayJson[i];
                if (json_is_string(stringJson.get()))
                    row.insertTxid(json_string_value(stringJson.get()));
            }

            row.lastCheck = addressJson.lastCheck();
            if (now < nextCheck(address, row))
                row.checkedOnce = true;

            if (addressJson.stateHashOk())
                row.stateHash = addressJson.stateHash();

            rows_[address] = row;
        }
    }
    updateInternal();

    return Status();
}

Status
AddressCache::save(JsonObject &json)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    CacheJson cacheJson(json);

    JsonArray addressesJson;
    for (const auto &row: rows_)
    {
        if (row.second.sweep)
            continue;

        JsonArray txidsJson;
        for (const auto &txid: row.second.txids)
            ABC_CHECK(txidsJson.append(json_string(txid.c_str())));

        AddressJson address;
        ABC_CHECK(address.addressSet(row.first));
        ABC_CHECK(address.txidsSet(txidsJson));
        ABC_CHECK(address.lastCheckSet(row.second.lastCheck));
        if (!row.second.stateHash.empty())
            ABC_CHECK(address.stateHashSet(row.second.stateHash));
        ABC_CHECK(addressesJson.append(address));
    }
    cacheJson.addressesSet(addressesJson);

    return Status();
}

std::pair<size_t, size_t>
AddressCache::progress() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    size_t done = 0;
    for (const auto &row: rows_)
        if (row.second.checkedOnce && row.second.complete)
            ++done;

    return std::pair<size_t, size_t>(done, rows_.size());
}

std::list<AddressStatus>
AddressCache::statuses(time_t &sleep) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    std::list<AddressStatus> out;

    time_t now = time(nullptr);
    time_t nextCheck = now;
    for (auto &row: rows_)
    {
        const auto s = status(row.first, row.second, now);
        if (s.needsCheck || s.missingTxids.size())
            out.push_back(std::move(s));

        if (now < s.nextCheck
                && (s.nextCheck < nextCheck || now == nextCheck))
            nextCheck = s.nextCheck;
    }

    sleep = nextCheck - now;
    out.sort();
    return out;
}

TxidSet
AddressCache::txids() const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return knownTxids_;
}

bool
AddressCache::stateHashDirty(const std::string &address,
                             const std::string &stateHash) const
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    const auto i = rows_.find(address);
    if (rows_.end() == i)
        return true;
    auto &row = i->second;
    if (row.stateHash.empty())
        return true;
    return row.stateHash != stateHash;
}

void
AddressCache::insert(const std::string &address, bool sweep)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    if (rows_.end() == rows_.find(address))
    {
        auto &row = rows_[address];
        row.sweep = sweep;

        if (wakeupCallback_)
            wakeupCallback_();
    }
}

void
AddressCache::prioritize(const std::string &address)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    priorityAddress_ = address;

    if (wakeupCallback_)
        wakeupCallback_();
}

void
AddressCache::update()
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    updateInternal();
}

void
AddressCache::update(const std::string &address, const TxidSet &txids,
                     const std::string &stateHash)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto &row = rows_[address];

    // Look for dropped txids:
    TxidSet drops;
    for (const auto &txid: row.txids)
    {
        if (!txids.count(txid) && txCache_.drop(txid))
        {
            drops.insert(txid);
            knownTxids_.erase(txid);
        }
    }

    // Remove the dropped txids from all addresses:
    for (const auto &txid: drops)
        for (auto &row: rows_)
            row.second.txids.erase(txid);

    // Look for new txids:
    for (const auto &txid: txids)
        if (!row.txids.count(txid))
            row.insertTxid(txid);

    // Update timestamp:
    row.lastCheck = time(nullptr);
    row.checkedOnce = true;

    if (!stateHash.empty())
        row.stateHash = stateHash;

    // Fire callbacks:
    updateInternal();
}

void
AddressCache::updateSpend(TxInfo &info)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);

    for (const auto &io: info.ios)
    {
        const auto i = rows_.find(io.address);
        if (rows_.end() != i)
            i->second.insertTxid(info.txid);
    }

    // Fire callbacks:
    updateInternal();
}

void
AddressCache::updateSubscribe(const std::string &address)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    auto &row = rows_[address];

    if (row.checkedOnce)
        row.lastCheck = time(nullptr);
}

void
AddressCache::wakeupCallbackSet(const Callback &callback)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    wakeupCallback_ = callback;
}

void
AddressCache::onTxSet(const TxidCallback &onTx)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    onTx_ = onTx;
}

void
AddressCache::onCompleteSet(const CompleteCallback &onComplete)
{
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    onComplete_ = onComplete;
}

time_t
AddressCache::nextCheck(const std::string &address, const AddressRow &row) const
{
    time_t period = periodDefault;
    if (priorityAddress_ == address)
        period = periodPriority;

    return row.lastCheck + period;
}

AddressStatus
AddressCache::status(const std::string &address, const AddressRow &row,
                     time_t now) const
{
    AddressStatus out{address};
    out.nextCheck = nextCheck(address, row);
    out.needsCheck = out.nextCheck <= now;
    out.count = row.txids.size();

    if (!row.complete)
        out.missingTxids = txCache_.missingTxids(row.txids);

    return out;
}

void
AddressCache::updateInternal()
{
    // Check for newly-completed transactions:
    for (auto &row: rows_)
    {
        // Skip rows that are already complete:
        if (row.second.complete)
            continue;

        row.second.complete = true;
        for (const auto &txid: row.second.txids)
        {
            // Skip transactions we already know about:
            if (knownTxids_.count(txid))
                continue;

            if (txCache_.missing(txid))
            {
                row.second.complete = false;
                continue;
            }

            // Don't notify the GUI about sweep transactions:
            if (!row.second.sweep)
            {
                knownTxids_.insert(txid);
                if (onTx_)
                    onTx_(txid);
            }
        }
    }

    // Check for newly-completed addresses:
    for (auto &row: rows_)
    {
        if (row.second.checkedOnce && row.second.complete
                && !row.second.knownComplete)
        {
            row.second.knownComplete = true;
            if (onComplete_)
                onComplete_(row.first);
        }
    }
}

} // namespace abcd
