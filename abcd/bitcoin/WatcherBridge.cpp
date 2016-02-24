/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "WatcherBridge.hpp"
#include "TxUpdater.hpp"
#include "Testnet.hpp"
#include "Utility.hpp"
#include "Watcher.hpp"
#include "../Tx.hpp"
#include "../spend/Broadcast.hpp"
#include "../spend/Inputs.hpp"
#include "../spend/Outputs.hpp"
#include "../spend/Spend.hpp"
#include "../util/Debug.hpp"
#include "../util/FileIO.hpp"
#include "../wallet/Address.hpp"
#include "../wallet/Wallet.hpp"
#include <algorithm>
#include <list>
#include <memory>
#include <unordered_map>

namespace abcd {

struct PendingSweep
{
    std::string address;
    abcd::wif_key key;
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
        watcher(wallet.txdb),
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
    ABC_CHECK(fileDelete(watcherPath(self)));
    return Status();
}

Status
watcherSave(Wallet &self)
{
    auto data = self.txdb.serialize();
    ABC_CHECK(fileSave(data, watcherPath(self)));

    return Status();
}

std::string
watcherPath(Wallet &self)
{
    return self.dir() + "watcher.ser";
}

Status
bridgeSweepKey(Wallet &self, DataSlice key, bool compressed)
{
    WatcherInfo *watcherInfo = nullptr;
    ABC_CHECK(watcherFind(watcherInfo, self));

    // Decode key and address:
    bc::ec_secret ec_key;
    if (ec_key.size() != key.size())
        return ABC_ERROR(ABC_CC_Error, "Bad key size");
    std::copy(key.begin(), key.end(), ec_key.data());
    bc::ec_point ec_addr = bc::secret_to_public_key(ec_key, compressed);
    bc::payment_address address(pubkeyVersion(),
                                bc::bitcoin_short_hash(ec_addr));

    // Start the sweep:
    PendingSweep sweep;
    sweep.address = address.encoded();
    sweep.key = abcd::wif_key{ec_key, compressed};
    sweep.done = false;
    watcherInfo->sweeping.push_back(sweep);
    watcherInfo->watcher.watch_address(address);

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
bridgeWatchAddress(const Wallet &self, const std::string &address)
{
    ABC_DebugLog("Watching %s for %s", address.c_str(), self.id().c_str());

    bc::payment_address addr;
    if (!addr.set_encoded(address))
        return ABC_ERROR(ABC_CC_ParseError, "Invalid address");

    WatcherInfo *watcherInfo = nullptr;
    ABC_CHECK(watcherFind(watcherInfo, self));
    watcherInfo->watcher.watch_address(addr);

    return Status();
}

Status
bridgePrioritizeAddress(Wallet &self, const char *szAddress)
{
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));

    bc::payment_address addr;
    if (szAddress)
    {
        if (!addr.set_encoded(szAddress))
            return ABC_ERROR(ABC_CC_ParseError, "Invalid address");
    }
    watcher->prioritize_address(addr);

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

/**
 * Filters a transaction list, removing any that aren't found in the
 * watcher database.
 * @param aTransactions The array to filter. This will be modified in-place.
 * @param pCount        The array length. This will be updated upon return.
 */
Status
bridgeFilterTransactions(Wallet &self,
                         tABC_TxInfo **aTransactions,
                         unsigned int *pCount)
{
    tABC_TxInfo *const *end = aTransactions + *pCount;
    tABC_TxInfo *const *si = aTransactions;
    tABC_TxInfo **di = aTransactions;

    while (si < end)
    {
        tABC_TxInfo *pTx = *si++;

        bc::hash_digest ntxid;
        if (!bc::decode_hash(ntxid, pTx->szID))
            return ABC_ERROR(ABC_CC_ParseError, "Bad ntxid");
        if (self.txdb.ntxidExists(ntxid))
        {
            *di++ = pTx;
        }
        else
        {
            ABC_TxFreeTransaction(pTx);
        }
    }
    *pCount = di - aTransactions;

    return Status();
}

static Status
bridgeDoSweep(WatcherInfo *watcherInfo,
              PendingSweep &sweep,
              tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    Address address;
    uint64_t funds = 0;
    abcd::unsigned_transaction utx;
    bc::transaction_output_type output;
    abcd::key_table keys;

    // Find utxos for this address:
    AddressSet addresses;
    addresses.insert(sweep.address);
    auto utxos = watcherInfo->wallet.txdb.get_utxos(addresses);

    // Bail out if there are no funds to sweep:
    if (!utxos.size())
    {
        // Tell the GUI if there were funds in the past:
        if (watcherInfo->wallet.txdb.has_history(sweep.address))
        {
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

    // Create a new receive request:
    watcherInfo->wallet.addresses.getNew(address);

    // Build a transaction:
    utx.tx.version = 1;
    utx.tx.locktime = 0;
    for (auto &utxo : utxos)
    {
        bc::transaction_input_type input;
        input.sequence = 0xffffffff;
        input.previous_output = utxo.point;
        funds += utxo.value;
        utx.tx.inputs.push_back(input);
    }
    if (10000 < funds)
        funds -= 10000; // Ugh, hard-coded mining fee
    if (outputIsDust(funds))
        return ABC_ERROR(ABC_CC_InsufficientFunds, "Not enough funds");
    output.value = funds;
    ABC_CHECK(outputScriptForAddress(output.script, address.address));
    utx.tx.outputs.push_back(output);

    // Now sign that:
    keys[sweep.address] = sweep.key;
    if (!gather_challenges(utx, watcherInfo->wallet))
        return ABC_ERROR(ABC_CC_SysError, "gather_challenges failed");
    if (!sign_tx(utx, keys))
        return ABC_ERROR(ABC_CC_SysError, "sign_tx failed");

    // Send:
    bc::data_chunk raw_tx(satoshi_raw_size(utx.tx));
    bc::satoshi_save(utx.tx, raw_tx.begin());
    ABC_CHECK(broadcastTx(watcherInfo->wallet, raw_tx));
    if (watcherInfo->wallet.txdb.insert(utx.tx))
        watcherSave(watcherInfo->wallet).log(); // Failure is not fatal

    // Save the transaction in the metadatabase:
    const auto txid = bc::encode_hash(bc::hash_transaction(utx.tx));
    const auto ntxid = bc::encode_hash(makeNtxid(utx.tx));
    ABC_CHECK(txSweepSave(watcherInfo->wallet, ntxid, txid, funds));

    // Done:
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

    const auto ntxid = bc::encode_hash(makeNtxid(tx));
    const auto txid = bc::encode_hash(bc::hash_transaction(tx));

    if (relevant)
    {
        ABC_DebugLog("New transaction %s", txid.c_str());
        ABC_CHECK(txReceiveTransaction(watcherInfo->wallet,
                                       ntxid, txid, addresses, fAsyncCallback, pData));
    }
    else
    {
        ABC_DebugLog("New (irrelevant) transaction %s", txid.c_str());
    }
    watcherSave(watcherInfo->wallet).log(); // Failure is not fatal

    return Status();
}

} // namespace abcd
