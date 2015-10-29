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
    bc::payment_address address;
    abcd::wif_key key;
    bool done;

    tABC_Sweep_Done_Callback fCallback;
    void *pData;
};

struct WatcherInfo
{
    WatcherInfo(Wallet &wallet):
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

static Status   bridgeDoSweep(WatcherInfo *watcherInfo, PendingSweep &sweep, tABC_BitCoin_Event_Callback fAsyncCallback, void *pData);
static void     bridgeQuietCallback(WatcherInfo *watcherInfo, tABC_BitCoin_Event_Callback fAsyncCallback, void *pData);
static Status   bridgeTxCallback(WatcherInfo *watcherInfo, const libbitcoin::transaction_type &tx, tABC_BitCoin_Event_Callback fAsyncCallback, void *pData);

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

Status
watcherFind(Watcher *&result, const Wallet &self)
{
    WatcherInfo *watcherInfo = nullptr;
    ABC_CHECK(watcherFind(watcherInfo, self));

    result = &watcherInfo->watcher;
    return Status();
}

static Status
watcherLoad(Wallet &self)
{
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));

    DataChunk data;
    ABC_CHECK(fileLoad(data, watcherPath(self)));
    if (!watcher->db().load(data))
        return ABC_ERROR(ABC_CC_Error, "Unable to load serialized watcher");

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
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));

    auto data = watcher->db().serialize();;
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
    sweep.address = address;
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

    watcherLoad(self).log(); // Failure is not fatal

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

    txCallback = [watcherInfo, fAsyncCallback, pData](const libbitcoin::transaction_type &tx)
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

tABC_CC ABC_BridgeWatchAddr(const Wallet &self,
                            const char *pubAddress,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_DebugLog("Watching %s for %s", pubAddress, self.id().c_str());
    bc::payment_address addr;

    WatcherInfo *watcherInfo = nullptr;
    ABC_CHECK_NEW(watcherFind(watcherInfo, self));

    if (!addr.set_encoded(pubAddress))
    {
        cc = ABC_CC_Error;
        ABC_DebugLog("Invalid pubAddress %s\n", pubAddress);
        goto exit;
    }
    watcherInfo->watcher.watch_address(addr);

exit:
    return cc;
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
        {
            cc = ABC_CC_Error;
            ABC_DebugLog("Invalid szAddress %s\n", szAddress);
            goto exit;
        }
    }

    watcher->prioritize_address(addr);

exit:
    return cc;
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

    watcher->disconnect();
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

tABC_CC
ABC_BridgeTxHeight(Wallet &self, const std::string &ntxid, int *height, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int height_;
    bc::hash_digest hash;

    Watcher *watcher = nullptr;
    ABC_CHECK_NEW(watcherFind(watcher, self));

    if (!bc::decode_hash(hash, ntxid))
        ABC_RET_ERROR(ABC_CC_ParseError, "Bad ntxid");
    if (!watcher->ntxidHeight(hash, height_))
    {
        cc = ABC_CC_Synchronizing;
    }
    *height = height_;
exit:
    return cc;
}

tABC_CC
ABC_BridgeTxBlockHeight(Wallet &self, int *height, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    Watcher *watcher = nullptr;
    ABC_CHECK_NEW(watcherFind(watcher, self));

    *height = watcher->db().last_height();
    if (*height == 0)
    {
        cc = ABC_CC_Synchronizing;
    }
exit:
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

    Watcher *watcher = nullptr;
    ABC_CHECK_NEW(watcherFind(watcher, self));

    while (si < end)
    {
        tABC_TxInfo *pTx = *si++;

        int height;
        bc::hash_digest txid;
        if (!bc::decode_hash(txid, pTx->szID))
            ABC_RET_ERROR(ABC_CC_ParseError, "Bad txid");
        if (watcher->ntxidHeight(txid, height))
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
    std::string malTxId, txId;

    // Find utxos for this address:
    auto utxos = watcherInfo->watcher.get_utxos(sweep.address);

    // Bail out if there are no funds to sweep:
    if (!utxos.size())
    {
        // Tell the GUI if there were funds in the past:
        if (watcherInfo->watcher.db().has_history(sweep.address))
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
    if (!gather_challenges(utx, watcherInfo->watcher))
        return ABC_ERROR(ABC_CC_SysError, "gather_challenges failed");
    if (!sign_tx(utx, keys))
        return ABC_ERROR(ABC_CC_SysError, "sign_tx failed");

    // Send:
    bc::data_chunk raw_tx(satoshi_raw_size(utx.tx));
    bc::satoshi_save(utx.tx, raw_tx.begin());
    ABC_CHECK(broadcastTx(raw_tx));

    // Save the transaction in the database:
    malTxId = bc::encode_hash(bc::hash_transaction(utx.tx));
    txId = ABC_BridgeNonMalleableTxId(utx.tx);
    ABC_CHECK_OLD(ABC_TxSweepSaveTransaction(watcherInfo->wallet,
        txId.c_str(), malTxId.c_str(), funds, &error));

    // Done:
    if (sweep.fCallback)
    {
        sweep.fCallback(ABC_CC_Ok, txId.c_str(), output.value);
    }
    else if (fAsyncCallback)
    {
        tABC_AsyncBitCoinInfo info;
        info.pData = pData;
        info.eventType = ABC_AsyncEventType_IncomingSweep;
        info.sweepSatoshi = output.value;
        info.szTxID = txId.c_str();
        fAsyncCallback(&info);
    }
    sweep.done = true;
    watcherInfo->watcher.send_tx(utx.tx);

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
    watcherInfo->sweeping.remove_if([](const PendingSweep& sweep) {
        return sweep.done; });
}

static Status
bridgeTxCallback(WatcherInfo *watcherInfo, const libbitcoin::transaction_type &tx,
    tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    auto addressStrings = watcherInfo->wallet.addresses.list();
    AddressSet myAddresses(addressStrings.begin(), addressStrings.end());

    bool relevant = false;
    for (const auto &i: tx.inputs)
    {
        bc::payment_address address;
        bc::extract(address, i.script);
        if (myAddresses.end() != myAddresses.find(address))
            relevant = true;
    }

    std::vector<std::string> addresses;
    for (const auto &o: tx.outputs)
    {
        bc::payment_address address;
        bc::extract(address, o.script);
        if (myAddresses.end() != myAddresses.find(address))
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
    for (auto& input: tx.inputs)
        input.script = bc::script_type();
    return bc::encode_hash(bc::hash_transaction(tx, bc::sighash::all));
}

Status
watcherBridgeRawTx(Wallet &self, const std::string &ntxid, DataChunk &result)
{
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));

    bc::hash_digest hash;
    if (!bc::decode_hash(hash, ntxid))
        return ABC_ERROR(ABC_CC_ParseError, "Bad ntxid");
    auto tx = watcher->db().ntxidLookup(hash);
    result.resize(satoshi_raw_size(tx));
    bc::satoshi_save(tx, result.begin());

    return Status();
}

} // namespace abcd
