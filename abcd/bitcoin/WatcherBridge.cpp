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
#include "../Tx.hpp"
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

static Status   bridgeDoSweep(WatcherInfo *watcherInfo, PendingSweep &sweep,
                              tABC_BitCoin_Event_Callback fAsyncCallback, void *pData);
static void     bridgeQuietCallback(WatcherInfo *watcherInfo,
                                    tABC_BitCoin_Event_Callback fAsyncCallback, void *pData);
static Status   bridgeTxCallback(WatcherInfo *watcherInfo,
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
        bridgeTxCallback(watcherInfo, tx, fCallback, pData).log();
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
bridgeDoSweep(WatcherInfo *watcherInfo,
              PendingSweep &sweep,
              tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    // Find utxos for this address:
    AddressSet addresses;
    addresses.insert(sweep.address);
    auto utxos = watcherInfo->wallet.txCache.get_utxos(addresses);

    // Bail out if there are no funds to sweep:
    if (!utxos.size())
    {
        // Tell the GUI if there were funds in the past:
        if (watcherInfo->wallet.txCache.has_history(sweep.address))
        {
            ABC_DebugLog("IncomingSweep callback: wallet %s, value: 0",
                         watcherInfo->wallet.id().c_str());
            tABC_AsyncBitCoinInfo info;
            info.pData = pData;
            info.eventType = ABC_AsyncEventType_IncomingSweep;
            Status().toError(info.status, ABC_HERE());
            info.szWalletUUID = watcherInfo->wallet.id().c_str();
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
    watcherInfo->wallet.addresses.getNew(address);
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
    ABC_CHECK(signTx(tx, watcherInfo->wallet.txCache, keys));

    // Send:
    bc::data_chunk raw_tx(satoshi_raw_size(tx));
    bc::satoshi_save(tx, raw_tx.begin());
    ABC_CHECK(broadcastTx(watcherInfo->wallet, raw_tx));
    if (watcherInfo->wallet.txCache.insert(tx))
        watcherSave(watcherInfo->wallet).log(); // Failure is not fatal

    // Save the transaction in the metadatabase:
    const auto txid = bc::encode_hash(bc::hash_transaction(tx));
    const auto ntxid = bc::encode_hash(makeNtxid(tx));
    ABC_CHECK(watcherInfo->wallet.addresses.markOutputs(txid));
    ABC_CHECK(txSweepSave(watcherInfo->wallet, ntxid, txid, funds));

    // Done:
    ABC_DebugLog("IncomingSweep callback: wallet %s, ntxid: %s, value: %d",
                 watcherInfo->wallet.id().c_str(), ntxid.c_str(), output.value);
    tABC_AsyncBitCoinInfo info;
    info.pData = pData;
    info.eventType = ABC_AsyncEventType_IncomingSweep;
    Status().toError(info.status, ABC_HERE());
    info.szWalletUUID = watcherInfo->wallet.id().c_str();
    info.szTxID = ntxid.c_str();
    info.sweepSatoshi = output.value;
    fAsyncCallback(&info);

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
        auto s = bridgeDoSweep(watcherInfo, sweep, fAsyncCallback, pData).log();
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
bridgeTxCallback(WatcherInfo *watcherInfo,
                 const libbitcoin::transaction_type &tx,
                 tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    const auto ntxid = bc::encode_hash(makeNtxid(tx));
    const auto txid = bc::encode_hash(bc::hash_transaction(tx));

    // Save the watcher database:
    ABC_CHECK(watcherInfo->wallet.addresses.markOutputs(txid));
    watcherSave(watcherInfo->wallet).log(); // Failure is not fatal

    // Update the metadata if the transaction is relevant:
    bool relevant = false;
    for (const auto &i: tx.inputs)
    {
        bc::payment_address address;
        bc::extract(address, i.script);
        if (watcherInfo->wallet.addresses.has(address.encoded()))
            relevant = true;
    }

    std::vector<std::string> addresses;
    for (const auto &o: tx.outputs)
    {
        bc::payment_address address;
        bc::extract(address, o.script);
        if (watcherInfo->wallet.addresses.has(address.encoded()))
            relevant = true;

        addresses.push_back(address.encoded());
    }

    if (relevant)
    {
        ABC_CHECK(txReceiveTransaction(watcherInfo->wallet,
                                       ntxid, txid, addresses,
                                       fAsyncCallback, pData));
    }
    else
    {
        ABC_DebugLog("New (irrelevant) transaction: txid %s", txid.c_str());
    }

    return Status();
}

} // namespace abcd
