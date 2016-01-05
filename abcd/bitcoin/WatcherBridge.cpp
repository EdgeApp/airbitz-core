/*
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 */

#include "WatcherBridge.hpp"
#include "TxUpdater.hpp"
#include "Testnet.hpp"
#include "Watcher.hpp"
#include "../Tx.hpp"
#include "../spend/Broadcast.hpp"
#include "../spend/Inputs.hpp"
#include "../spend/Outputs.hpp"
#include "../spend/Spend.hpp"
#include "../util/FileIO.hpp"
#include "../util/Util.hpp"
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

    tABC_Sweep_Done_Callback fCallback;
    void *pData;
};

struct WatcherInfo
{
    WatcherInfo(Wallet &wallet):
        watcher(wallet.txdb),
        wallet(wallet),
        parent_(wallet.shared_from_this())
    {
    }

    Watcher watcher;
    std::list<PendingSweep> sweeping;
    Wallet &wallet;

private:
    std::shared_ptr<Wallet> parent_;
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

tABC_CC ABC_BridgeSweepKey(Wallet &self,
                           tABC_U08Buf key,
                           bool compressed,
                           tABC_Sweep_Done_Callback fCallback,
                           void *pData,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    bc::ec_secret ec_key;
    bc::ec_point ec_addr;
    bc::payment_address address;
    PendingSweep sweep;

    WatcherInfo *watcherInfo = nullptr;
    ABC_CHECK_NEW(watcherFind(watcherInfo, self));

    // Decode key and address:
    ABC_CHECK_ASSERT(key.size() == ec_key.size(),
                     ABC_CC_Error, "Bad key size");
    std::copy(key.begin(), key.end(), ec_key.data());
    ec_addr = bc::secret_to_public_key(ec_key, compressed);
    address.set(pubkeyVersion(), bc::bitcoin_short_hash(ec_addr));

    // Start the sweep:
    sweep.address = address.encoded();
    sweep.key = abcd::wif_key{ec_key, compressed};
    sweep.done = false;
    sweep.fCallback = fCallback;
    sweep.pData = pData;
    watcherInfo->sweeping.push_back(sweep);
    watcherInfo->watcher.watch_address(address);

exit:
    return cc;
}

tABC_CC ABC_BridgeWatcherStart(Wallet &self,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string id = self.id();

    if (watchers_.end() != watchers_.find(id))
        ABC_RET_ERROR(ABC_CC_Error, ("Watcher already exists for " + id).c_str());

    watchers_[id].reset(new WatcherInfo(self));

exit:
    return cc;
}

tABC_CC ABC_BridgeWatcherLoop(Wallet &self,
                              tABC_BitCoin_Event_Callback fAsyncCallback,
                              void *pData,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    Watcher::block_height_callback heightCallback;
    Watcher::tx_callback txCallback;
    Watcher::quiet_callback on_quiet;

    WatcherInfo *watcherInfo = nullptr;
    ABC_CHECK_NEW(watcherFind(watcherInfo, self));

    txCallback = [watcherInfo, fAsyncCallback,
                  pData](const libbitcoin::transaction_type &tx)
    {
        bridgeTxCallback(watcherInfo, tx, fAsyncCallback, pData).log();
    };
    watcherInfo->watcher.set_tx_callback(txCallback);

    heightCallback = [watcherInfo, fAsyncCallback, pData](const size_t height)
    {
        if (fAsyncCallback)
        {
            tABC_AsyncBitCoinInfo info;
            info.eventType = ABC_AsyncEventType_BlockHeightChange;
            info.pData = pData;
            info.szDescription = "Block height change";
            fAsyncCallback(&info);
        }
        watcherSave(watcherInfo->wallet).log(); // Failure is not fatal
    };
    watcherInfo->watcher.set_height_callback(heightCallback);

    on_quiet = [watcherInfo, fAsyncCallback, pData]()
    {
        bridgeQuietCallback(watcherInfo, fAsyncCallback, pData);
    };
    watcherInfo->watcher.set_quiet_callback(on_quiet);

    watcherInfo->watcher.loop();

    watcherInfo->watcher.set_quiet_callback(nullptr);
    watcherInfo->watcher.set_height_callback(nullptr);
    watcherInfo->watcher.set_tx_callback(nullptr);

exit:
    return cc;
}

tABC_CC ABC_BridgeWatcherConnect(Wallet &self, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    Watcher *watcher = nullptr;
    ABC_CHECK_NEW(watcherFind(watcher, self));
    watcher->connect();

exit:
    return cc;
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

tABC_CC ABC_BridgePrioritizeAddress(Wallet &self,
                                    const char *szAddress,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    bc::payment_address addr;

    Watcher *watcher = nullptr;
    ABC_CHECK_NEW(watcherFind(watcher, self));

    if (szAddress)
    {
        if (!addr.set_encoded(szAddress))
            ABC_RET_ERROR(ABC_CC_ParseError, "Invalid address");
    }

    watcher->prioritize_address(addr);

exit:
    return cc;
}

Status
watcherSend(Wallet &self, StatusCallback status, DataSlice tx)
{
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));

    watcher->sendTx(status, tx);

    return Status();
}

tABC_CC ABC_BridgeWatcherDisconnect(Wallet &self, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    Watcher *watcher = nullptr;
    ABC_CHECK_NEW(watcherFind(watcher, self));

    watcher->disconnect();

exit:
    return cc;
}

tABC_CC ABC_BridgeWatcherStop(Wallet &self, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    Watcher *watcher = nullptr;
    ABC_CHECK_NEW(watcherFind(watcher, self));

    watcher->stop();

exit:
    return cc;
}

tABC_CC ABC_BridgeWatcherDelete(Wallet &self, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    watcherSave(self).log(); // Failure is not fatal
    watchers_.erase(self.id());

    return cc;
}

/**
 * Filters a transaction list, removing any that aren't found in the
 * watcher database.
 * @param aTransactions The array to filter. This will be modified in-place.
 * @param pCount        The array length. This will be updated upon return.
 */
tABC_CC ABC_BridgeFilterTransactions(Wallet &self,
                                     tABC_TxInfo **aTransactions,
                                     unsigned int *pCount,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_TxInfo *const *end = aTransactions + *pCount;
    tABC_TxInfo *const *si = aTransactions;
    tABC_TxInfo **di = aTransactions;

    while (si < end)
    {
        tABC_TxInfo *pTx = *si++;

        bc::hash_digest ntxid;
        if (!bc::decode_hash(ntxid, pTx->szID))
            ABC_RET_ERROR(ABC_CC_ParseError, "Bad ntxid");
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

exit:
    return cc;
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
            if (sweep.fCallback)
            {
                sweep.fCallback(ABC_CC_Ok, NULL, 0);
            }
            else if (fAsyncCallback)
            {
                tABC_AsyncBitCoinInfo info;
                info.pData = pData;
                info.eventType = ABC_AsyncEventType_IncomingSweep;
                info.sweepSatoshi = 0;
                info.szTxID = nullptr;
                fAsyncCallback(&info);
            }
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
    if (watcherInfo->wallet.txdb.insert(utx.tx, TxState::unconfirmed))
        watcherSave(watcherInfo->wallet).log(); // Failure is not fatal

    // Save the transaction in the metadatabase:
    const auto txid = bc::encode_hash(bc::hash_transaction(utx.tx));
    const auto ntxid = ABC_BridgeNonMalleableTxId(utx.tx);
    ABC_CHECK(txSweepSave(watcherInfo->wallet, ntxid, txid, funds));

    // Done:
    if (sweep.fCallback)
    {
        sweep.fCallback(ABC_CC_Ok, ntxid.c_str(), output.value);
    }
    else if (fAsyncCallback)
    {
        tABC_AsyncBitCoinInfo info;
        info.pData = pData;
        info.eventType = ABC_AsyncEventType_IncomingSweep;
        info.sweepSatoshi = output.value;
        info.szTxID = ntxid.c_str();
        fAsyncCallback(&info);
    }
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
            if (sweep.fCallback)
                sweep.fCallback(s.value(), NULL, 0);
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

    const auto ntxid = ABC_BridgeNonMalleableTxId(tx);
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

std::string
ABC_BridgeNonMalleableTxId(bc::transaction_type tx)
{
    for (auto &input: tx.inputs)
        input.script = bc::script_type();
    return bc::encode_hash(bc::hash_transaction(tx, bc::sighash::all));
}

} // namespace abcd
