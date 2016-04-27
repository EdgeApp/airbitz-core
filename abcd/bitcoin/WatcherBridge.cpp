/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "WatcherBridge.hpp"
#include "Watcher.hpp"
#include "cache/Cache.hpp"
#include "../spend/Sweep.hpp"
#include "../util/Debug.hpp"
#include "../util/FileIO.hpp"
#include "../wallet/Receive.hpp"
#include "../wallet/Wallet.hpp"
#include <algorithm>
#include <list>
#include <memory>

namespace abcd {

struct PendingSweep
{
    std::string address;
    std::string key;
};

struct WatcherInfo
{
private:
    // This needs to come first, since the watcher relies on it's lifetime:
    std::shared_ptr<Wallet> parent_;

public:
    WatcherInfo(Wallet &wallet):
        parent_(wallet.shared_from_this()),
        watcher(wallet.cache),
        wallet(wallet)
    {
    }

    Watcher watcher;
    Wallet &wallet;
    std::list<PendingSweep> sweeping;
};

static std::map<std::string, std::unique_ptr<WatcherInfo>> watchers_;

static void     bridgeQuietCallback(WatcherInfo *watcherInfo,
                                    tABC_BitCoin_Event_Callback fAsyncCallback, void *pData);
static Status   bridgeTxCallback(Wallet &wallet,
                                 const libbitcoin::transaction_type &tx,
                                 tABC_BitCoin_Event_Callback fAsyncCallback, void *pData);

static Status
watcherFind(WatcherInfo *&result, const Wallet &self)
{
    std::string id = self.id();
    auto row = watchers_.find(id);
    if (row == watchers_.end())
        return ABC_ERROR(ABC_CC_Synchronizing, "Cannot find watcher for " + id);

    result = row->second.get();
    return Status();
}

static Status
watcherFind(Watcher *&result, const Wallet &self)
{
    WatcherInfo *watcherInfo = nullptr;
    ABC_CHECK(watcherFind(watcherInfo, self));

    result = &watcherInfo->watcher;
    return Status();
}

Status
bridgeSweepKey(Wallet &self, const std::string &wif,
               const std::string &address)
{
    WatcherInfo *watcherInfo = nullptr;
    ABC_CHECK(watcherFind(watcherInfo, self));

    // Start the sweep:
    PendingSweep sweep;
    sweep.address = address;
    sweep.key = wif;
    watcherInfo->sweeping.push_back(sweep);
    self.cache.addresses.insert(sweep.address);

    return Status();
}

Status
bridgeWatcherStart(Wallet &self)
{
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
    WatcherInfo *watcherInfo = nullptr;
    ABC_CHECK(watcherFind(watcherInfo, self));

    // Set up the address-changed callback:
    auto wakeupCallback = [watcherInfo]()
    {
        watcherInfo->watcher.sendWakeup();
    };
    self.cache.addresses.wakeupCallbackSet(wakeupCallback);

    // Set up the address-synced callback:
    auto doneCallback = [watcherInfo, fCallback, pData]()
    {
        ABC_DebugLog("AddressCheckDone callback: wallet %s",
                     watcherInfo->wallet.id().c_str());
        tABC_AsyncBitCoinInfo info;
        info.pData = pData;
        info.eventType = ABC_AsyncEventType_AddressCheckDone;
        Status().toError(info.status, ABC_HERE());
        info.szWalletUUID = watcherInfo->wallet.id().c_str();
        info.szTxID = nullptr;
        info.sweepSatoshi = 0;
        fCallback(&info);
    };
    self.cache.addresses.doneCallbackSet(doneCallback);

    // Set up new-transaction callback:
    auto txCallback = [watcherInfo, fCallback, pData]
                      (const libbitcoin::transaction_type &tx)
    {
        bridgeTxCallback(watcherInfo->wallet, tx, fCallback, pData).log();
    };
    watcherInfo->watcher.set_tx_callback(txCallback);

    // Set up new-block callback:
    auto heightCallback = [watcherInfo, fCallback, pData](const size_t height)
    {
        // Update the GUI:
        ABC_DebugLog("BlockHeightChange callback: wallet %s",
                     watcherInfo->wallet.id().c_str());
        tABC_AsyncBitCoinInfo info;
        info.pData = pData;
        info.eventType = ABC_AsyncEventType_BlockHeightChange;
        Status().toError(info.status, ABC_HERE());
        info.szWalletUUID = watcherInfo->wallet.id().c_str();
        info.szTxID = nullptr;
        info.sweepSatoshi = 0;
        fCallback(&info);

        watcherInfo->wallet.cache.save().log(); // Failure is fine
    };
    watcherInfo->watcher.set_height_callback(heightCallback);

    // Set up sweep-trigger callback:
    auto onQuiet = [watcherInfo, fCallback, pData]()
    {
        bridgeQuietCallback(watcherInfo, fCallback, pData);
    };
    watcherInfo->watcher.set_quiet_callback(onQuiet);

    // Do the loop:
    watcherInfo->watcher.loop();

    // Cancel all callbacks:
    watcherInfo->watcher.set_quiet_callback(nullptr);
    watcherInfo->watcher.set_height_callback(nullptr);
    watcherInfo->watcher.set_tx_callback(nullptr);
    self.cache.addresses.wakeupCallbackSet(nullptr);
    self.cache.addresses.doneCallbackSet(nullptr);

    return Status();
}

Status
bridgeWatcherConnect(Wallet &self)
{
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));

    watcher->connect();

    return Status();
}

Status
watcherSend(Wallet &self, StatusCallback status, DataSlice tx)
{
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));

    watcher->sendTx(status, tx);

    return Status();
}

Status
bridgeWatcherDisconnect(Wallet &self)
{
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));

    watcher->disconnect();

    return Status();
}

Status
bridgeWatcherStop(Wallet &self)
{
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));

    watcher->stop();

    return Status();
}

Status
bridgeWatcherDelete(Wallet &self)
{
    self.cache.save().log(); // Failure is fine
    watchers_.erase(self.id());

    return Status();
}

static void
bridgeQuietCallback(WatcherInfo *watcherInfo,
                    tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    auto &wallet = watcherInfo->wallet;

    // If we are sweeping any keys, do that now:
    auto i = watcherInfo->sweeping.begin();
    while (watcherInfo->sweeping.end() != i)
    {
        if (wallet.cache.txs.has_history(i->address))
        {
            // Remove the sweep from the list:
            auto sweep = *i;
            i = watcherInfo->sweeping.erase(i);

            sweepOnComplete(wallet, sweep.address, sweep.key,
                            fAsyncCallback, pData);
        }
        else
        {
            ++i;
        }
    }
}

static Status
bridgeTxCallback(Wallet &wallet,
                 const libbitcoin::transaction_type &tx,
                 tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    const auto info = wallet.cache.txs.txInfo(tx);

    // Does this transaction concern us?
    if (wallet.cache.txs.isRelevant(tx, wallet.addresses.list()))
    {
        ABC_CHECK(onReceive(wallet, info, fAsyncCallback, pData));
    }
    else
    {
        ABC_DebugLog("New (irrelevant) transaction: wallet %s, txid: %s",
                     wallet.id().c_str(), info.txid.c_str());
    }

    return Status();
}

} // namespace abcd
