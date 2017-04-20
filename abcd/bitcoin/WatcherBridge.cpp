/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "WatcherBridge.hpp"
#include "Watcher.hpp"
#include "cache/Cache.hpp"
#include "spend/Sweep.hpp"
#include "../Context.hpp"
#include "../util/Debug.hpp"
#include "../util/FileIO.hpp"
#include "../wallet/Receive.hpp"
#include "../wallet/Wallet.hpp"
#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <mutex>

namespace abcd {

struct WatcherInfo
{
private:
    // This needs to come first, since the watcher relies on it's lifetime:
    std::shared_ptr<Wallet> parent_;

public:
    WatcherInfo(Wallet &wallet):
        parent_(wallet.shared_from_this()),
        watcher(wallet),
        wallet(wallet)
    {
    }

    Watcher watcher;
    Wallet &wallet;
    std::map<std::string, std::string> sweeping; // address to key

    tABC_BitCoin_Event_Callback fCallback;
    void *pData;
};

static std::mutex watchersMutex_;
static std::map<std::string, std::shared_ptr<WatcherInfo>> watchers_;

/**
 * Returns a list of all watchers currently running.
 */
static std::list<std::shared_ptr<WatcherInfo> >
listWatchers()
{
    std::list<std::shared_ptr<WatcherInfo>> out;
    std::lock_guard<std::mutex> lock(watchersMutex_);

    for (auto &watcher: watchers_)
    {
        out.push_back(watcher.second);
    }
    return out;
}

/**
 * Tells all running watchers that height has changed.
 * This is a temporary hack until we gain support for app-wide callbacks.
 */
static void
onHeight(size_t height)
{
    for (auto &watcher: listWatchers())
    {
        if (watcher->fCallback)
        {
            ABC_DebugLog("BlockHeightChange callback: wallet %s",
                         watcher->wallet.id().c_str());
            tABC_AsyncBitCoinInfo info;
            info.pData = watcher->pData;
            info.eventType = ABC_AsyncEventType_BlockHeightChange;
            Status().toError(info.status, ABC_HERE());
            info.szWalletUUID = watcher->wallet.id().c_str();
            info.szTxID = nullptr;
            info.sweepSatoshi = 0;
            watcher->fCallback(&info);
        }
    }
}

/**
 * Tells all running watchers that a new header was found.
 * This is a temporary hack until we gain support for app-wide callbacks.
 */
static void
onHeader(void)
{
    for (auto &watcher: listWatchers())
    {
        if (watcher->fCallback)
        {
            ABC_DebugLog("BlockHeader callback: wallet %s",
                         watcher->wallet.id().c_str());
            tABC_AsyncBitCoinInfo info;
            info.pData = watcher->pData;
            info.eventType = ABC_AsyncEventType_TransactionUpdate;
            Status().toError(info.status, ABC_HERE());
            info.szWalletUUID = watcher->wallet.id().c_str();

            // XXX Todo: Look up TxIDs that actually have a matching height and
            // send a notification for each one OR send one notification with all
            // affected TxIDs in a std::set.
            info.szTxID = nullptr;
            info.sweepSatoshi = 0;
            watcher->fCallback(&info);

            break;
        }
    }
}

/**
 * Called when an address is completely loaded into the cache.
 */
static void
bridgeOnComplete(std::shared_ptr<WatcherInfo> watcherInfo,
                 const std::string &address,
                 tABC_BitCoin_Event_Callback fCallback, void *pData)
{
    auto &wallet = watcherInfo->wallet;

    // If we are sweeping this address, do that now:
    auto i = watcherInfo->sweeping.find(address);
    if (watcherInfo->sweeping.end() != i)
    {
        // We need to do this first, since the actual send
        // triggers another `onComplete` callback for the sweep address:
        auto sweep = *i;
        watcherInfo->sweeping.erase(i);

        sweepOnComplete(wallet, sweep.first, sweep.second, fCallback, pData);
    }

    // Send the AddressCheckDone callback if its time:
    const auto p = wallet.cache.addresses.progress();
    if (p.first == p.second)
    {
        ABC_DebugLog("AddressCheckDone callback: wallet %s",
                     wallet.id().c_str());
        tABC_AsyncBitCoinInfo info;
        info.pData = pData;
        info.eventType = ABC_AsyncEventType_AddressCheckDone;
        Status().toError(info.status, ABC_HERE());
        info.szWalletUUID = wallet.id().c_str();
        info.szTxID = nullptr;
        info.sweepSatoshi = 0;
        wallet.cache.addressCheckDoneSet();
        wallet.cache.save();
        fCallback(&info);
    }
}

static Status
watcherFind(std::shared_ptr<WatcherInfo> &result, const Wallet &self)
{
    std::lock_guard<std::mutex> lock(watchersMutex_);
    std::string id = self.id();
    auto row = watchers_.find(id);
    if (row == watchers_.end())
        return ABC_ERROR(ABC_CC_Synchronizing, "Cannot find watcher for " + id);

    result = row->second;
    return Status();
}

Status
bridgeSweepKey(Wallet &self, const std::string &wif,
               const std::string &address)
{
    std::shared_ptr<WatcherInfo> watcherInfo;
    ABC_CHECK(watcherFind(watcherInfo, self));

    // Start the sweep:
    watcherInfo->sweeping[address] = wif;
    self.cache.addresses.insert(address, true);

    return Status();
}

Status
bridgeWatcherStart(Wallet &self)
{
    std::lock_guard<std::mutex> lock(watchersMutex_);
    if (watchers_.end() != watchers_.find(self.id()))
        return ABC_ERROR(ABC_CC_Error,
                         "Watcher already exists for " + self.id());

    watchers_[self.id()].reset(new WatcherInfo(self));

    return Status();
}

Status
bridgeWatcherLoop(Wallet &self,
                  tABC_BitCoin_Event_Callback fCallback,
                  void *pData)
{
    std::shared_ptr<WatcherInfo> watcherInfo;
    ABC_CHECK(watcherFind(watcherInfo, self));

    // Set up new-block callback:
    watcherInfo->fCallback = fCallback;
    watcherInfo->pData = pData;
    gContext->blockCache.onHeightSet(onHeight);
    gContext->blockCache.onHeaderSet(onHeader);

    // Set up the address-changed callback:
    auto wakeupCallback = [watcherInfo]()
    {
        watcherInfo->watcher.sendWakeup();
    };
    self.cache.addresses.wakeupCallbackSet(wakeupCallback);

    // Set up the new-transaction callback:
    auto onTx = [watcherInfo, fCallback, pData]
                (const std::string &txid)
    {
        ABC_DebugLog("**************************************************************");
        ABC_DebugLog("**** GUI Notified of NEW TRANSACTION txid %s", txid.c_str());
        ABC_DebugLog("**************************************************************\n");

        TxInfo info;
        if (watcherInfo->wallet.cache.txs.info(info, txid).log())
            onReceive(watcherInfo->wallet, info, fCallback, pData).log();
    };
    self.cache.addresses.onTxSet(onTx);

    // Set up the address-completed callback:
    auto onComplete = [watcherInfo, fCallback, pData]
                      (const std::string &address)
    {
        bridgeOnComplete(watcherInfo, address, fCallback, pData);
    };
    self.cache.addresses.onCompleteSet(onComplete);

    // Do the loop:
    watcherInfo->watcher.loop();

    // Cancel all callbacks:
    self.cache.addresses.wakeupCallbackSet(nullptr);
    self.cache.addresses.onTxSet(nullptr);
    self.cache.addresses.onCompleteSet(nullptr);
    watcherInfo->fCallback = nullptr;
    watcherInfo->pData = nullptr;

    return Status();
}

Status
bridgeWatcherConnect(Wallet &self)
{
    std::shared_ptr<WatcherInfo> watcherInfo;
    ABC_CHECK(watcherFind(watcherInfo, self));

    watcherInfo->watcher.connect();

    return Status();
}

Status
watcherSend(Wallet &self, StatusCallback status, DataSlice tx)
{
    std::shared_ptr<WatcherInfo> watcherInfo;
    ABC_CHECK(watcherFind(watcherInfo, self));

    watcherInfo->watcher.sendTx(status, tx);

    return Status();
}

Status
bridgeWatcherDisconnect(Wallet &self)
{
    std::shared_ptr<WatcherInfo> watcherInfo;
    ABC_CHECK(watcherFind(watcherInfo, self));

    watcherInfo->watcher.disconnect();

    return Status();
}

Status
bridgeWatcherStop(Wallet &self)
{
    std::shared_ptr<WatcherInfo> watcherInfo;
    ABC_CHECK(watcherFind(watcherInfo, self));

    watcherInfo->watcher.stop();

    return Status();
}

Status
bridgeWatcherDelete(Wallet &self)
{
    self.cache.save().log(); // Failure is fine

    std::lock_guard<std::mutex> lock(watchersMutex_);
    watchers_.erase(self.id());

    return Status();
}

} // namespace abcd
