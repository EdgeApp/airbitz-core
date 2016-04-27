/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "WatcherBridge.hpp"
#include "Testnet.hpp"
#include "Utility.hpp"
#include "Watcher.hpp"
#include "../Context.hpp"
#include "../exchange/ExchangeCache.hpp"
#include "../spend/Broadcast.hpp"
#include "../spend/Inputs.hpp"
#include "../spend/Outputs.hpp"
#include "../util/Debug.hpp"
#include "../util/FileIO.hpp"
#include "../wallet/Wallet.hpp"
#include <algorithm>
#include <list>
#include <memory>
#include <unordered_map>

namespace abcd {

struct PendingSweep
{
    std::string address;
    std::string key;
    bool done;
};

struct WatcherInfo
{
private:
    // This needs to come first, since the watcher relies on it's lifetime:
    std::shared_ptr<Wallet> parent_;

public:
    WatcherInfo(Wallet &wallet):
        parent_(wallet.shared_from_this()),
        watcher(wallet.txCache, wallet.addressCache),
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
watcherDeleteCache(Wallet &self)
{
    ABC_CHECK(fileDelete(self.paths.watcherPath()));
    return Status();
}

Status
watcherSave(Wallet &self)
{
    auto data = self.txCache.serialize();
    ABC_CHECK(fileSave(data, self.paths.watcherPath()));

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
    sweep.done = false;
    watcherInfo->sweeping.push_back(sweep);
    self.addressCache.insert(sweep.address);

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
    self.addressCache.wakeupCallbackSet(wakeupCallback);

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
    self.addressCache.doneCallbackSet(doneCallback);

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

        watcherSave(watcherInfo->wallet).log(); // Failure is not fatal
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
    self.addressCache.wakeupCallbackSet(nullptr);
    self.addressCache.doneCallbackSet(nullptr);

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
    watcherSave(self).log(); // Failure is not fatal
    watchers_.erase(self.id());

    return Status();
}

static Status
bridgeDoSweep(Wallet &wallet, PendingSweep &sweep,
              tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    // Find utxos for this address:
    AddressSet addresses;
    addresses.insert(sweep.address);
    auto utxos = wallet.txCache.get_utxos(addresses);

    // Bail out if there are no funds to sweep:
    if (!utxos.size())
    {
        // Tell the GUI if there were funds in the past:
        if (wallet.txCache.has_history(sweep.address))
        {
            ABC_DebugLog("IncomingSweep callback: wallet %s, value: 0",
                         wallet.id().c_str());
            tABC_AsyncBitCoinInfo info;
            info.pData = pData;
            info.eventType = ABC_AsyncEventType_IncomingSweep;
            Status().toError(info.status, ABC_HERE());
            info.szWalletUUID = wallet.id().c_str();
            info.szTxID = nullptr;
            info.sweepSatoshi = 0;
            fAsyncCallback(&info);

            sweep.done = true;
        }
        return Status();
    }

    // Build a transaction:
    bc::transaction_type tx;
    tx.version = 1;
    tx.locktime = 0;

    // Set up the output:
    Address address;
    wallet.addresses.getNew(address);
    bc::transaction_output_type output;
    ABC_CHECK(outputScriptForAddress(output.script, address.address));
    tx.outputs.push_back(output);

    // Set up the inputs:
    uint64_t fee, funds;
    ABC_CHECK(inputsPickMaximum(fee, funds, tx, utxos));
    if (outputIsDust(funds))
        return ABC_ERROR(ABC_CC_InsufficientFunds, "Not enough funds");
    tx.outputs[0].value = funds;

    // Now sign that:
    KeyTable keys;
    keys[sweep.address] = sweep.key;
    ABC_CHECK(signTx(tx, wallet.txCache, keys));

    // Send:
    bc::data_chunk raw_tx(satoshi_raw_size(tx));
    bc::satoshi_save(tx, raw_tx.begin());
    ABC_CHECK(broadcastTx(wallet, raw_tx));

    // Calculate transaction information:
    const auto info = wallet.txCache.txInfo(tx, wallet.addresses.list());

    // Save the transaction metadata:
    TxMeta meta;
    meta.ntxid = info.ntxid;
    meta.txid = info.txid;
    meta.timeCreation = time(nullptr);
    meta.internal = true;
    meta.metadata.amountSatoshi = funds;
    meta.metadata.amountFeesAirbitzSatoshi = 0;
    ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                  meta.metadata.amountCurrency, info.balance,
                  static_cast<Currency>(wallet.currency())));
    ABC_CHECK(wallet.txs.save(meta));

    // Update the transaction cache:
    if (wallet.txCache.insert(tx))
        watcherSave(wallet).log(); // Failure is not fatal
    wallet.balanceDirty();
    ABC_CHECK(wallet.addresses.markOutputs(info.ios));

    // Done:
    ABC_DebugLog("IncomingSweep callback: wallet %s, txid: %s, value: %d",
                 wallet.id().c_str(), info.txid.c_str(), output.value);
    tABC_AsyncBitCoinInfo async;
    async.pData = pData;
    async.eventType = ABC_AsyncEventType_IncomingSweep;
    Status().toError(async.status, ABC_HERE());
    async.szWalletUUID = wallet.id().c_str();
    async.szTxID = info.txid.c_str();
    async.sweepSatoshi = output.value;
    fAsyncCallback(&async);

    sweep.done = true;

    return Status();
}

static void
bridgeQuietCallback(WatcherInfo *watcherInfo,
                    tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    // If we are sweeping any keys, do that now:
    for (auto &sweep: watcherInfo->sweeping)
    {
        auto s = bridgeDoSweep(watcherInfo->wallet, sweep, fAsyncCallback, pData).log();
        if (!s)
        {
            ABC_DebugLog("IncomingSweep callback: wallet %s, status: %d",
                         watcherInfo->wallet.id().c_str(), s.value());
            tABC_AsyncBitCoinInfo info;
            info.pData = pData;
            info.eventType = ABC_AsyncEventType_IncomingSweep;
            s.toError(info.status, ABC_HERE());
            info.szWalletUUID = watcherInfo->wallet.id().c_str();
            info.szTxID = nullptr;
            info.sweepSatoshi = 0;
            fAsyncCallback(&info);

            sweep.done = true;
        }
    }

    // Remove completed ones:
    watcherInfo->sweeping.remove_if([](const PendingSweep& sweep)
    {
        return sweep.done;
    });
}

static Status
bridgeTxCallback(Wallet &wallet,
                 const libbitcoin::transaction_type &tx,
                 tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    const auto addresses = wallet.addresses.list();
    const auto info = wallet.txCache.txInfo(tx, addresses);

    // Does this transaction concern us?
    if (wallet.txCache.isRelevant(tx, addresses))
    {
        // Does the transaction already exist?
        TxMeta meta;
        if (!wallet.txs.get(meta, info.ntxid))
        {
            meta.ntxid = info.ntxid;
            meta.txid = info.txid;
            meta.timeCreation = time(nullptr);
            meta.internal = false;

            // Grab metadata from the address:
            TxMetadata metadata;
            for (const auto &io: info.ios)
            {
                Address address;
                if (wallet.addresses.get(address, io.address))
                    meta.metadata = address.metadata;
            }
            meta.metadata.amountSatoshi = info.balance;
            meta.metadata.amountFeesMinersSatoshi = info.fee;
            ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                          meta.metadata.amountCurrency, info.balance,
                          static_cast<Currency>(wallet.currency())));

            // Save the metadata:
            ABC_CHECK(wallet.txs.save(meta));

            // Update the transaction cache:
            watcherSave(wallet).log(); // Failure is not fatal
            wallet.balanceDirty();
            ABC_CHECK(wallet.addresses.markOutputs(info.ios));

            // Update the GUI:
            ABC_DebugLog("IncomingBitCoin callback: wallet %s, txid: %s",
                         wallet.id().c_str(), info.txid.c_str());
            tABC_AsyncBitCoinInfo async;
            async.pData = pData;
            async.eventType = ABC_AsyncEventType_IncomingBitCoin;
            Status().toError(async.status, ABC_HERE());
            async.szWalletUUID = wallet.id().c_str();
            async.szTxID = info.txid.c_str();
            async.sweepSatoshi = 0;
            fAsyncCallback(&async);
        }
        else
        {
            // Update the transaction cache:
            watcherSave(wallet).log(); // Failure is not fatal
            wallet.balanceDirty();
            ABC_CHECK(wallet.addresses.markOutputs(info.ios));

            // Update the GUI:
            ABC_DebugLog("BalanceUpdate callback: wallet %s, txid: %s",
                         wallet.id().c_str(), info.txid.c_str());
            tABC_AsyncBitCoinInfo async;
            async.pData = pData;
            async.eventType = ABC_AsyncEventType_BalanceUpdate;
            Status().toError(async.status, ABC_HERE());
            async.szWalletUUID = wallet.id().c_str();
            async.szTxID = info.txid.c_str();
            async.sweepSatoshi = 0;
            fAsyncCallback(&async);
        }
    }
    else
    {
        ABC_DebugLog("New (irrelevant) transaction:  wallet %s, txid: %s",
                     wallet.id().c_str(), info.txid.c_str());
    }

    return Status();
}

} // namespace abcd
