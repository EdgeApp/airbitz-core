/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Sweep.hpp"
#include "Broadcast.hpp"
#include "Inputs.hpp"
#include "Outputs.hpp"
#include "../Context.hpp"
#include "../bitcoin/cache/Cache.hpp"
#include "../exchange/ExchangeCache.hpp"
#include "../util/Debug.hpp"
#include "../wallet/Wallet.hpp"

namespace abcd {

/**
 * Performs the actual sweep.
 */
static Status
sweepSend(Wallet &wallet,
          const std::string &address, const std::string &wif,
          tABC_BitCoin_Event_Callback fCallback, void *pData)
{
    // Find utxos for this address:
    AddressSet addresses;
    addresses.insert(address);
    auto utxos = wallet.cache.txs.get_utxos(addresses);

    // Bail out if there are no funds to sweep:
    if (!utxos.size())
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
        fCallback(&info);

        return Status();
    }

    // Build a transaction:
    bc::transaction_type tx;
    tx.version = 1;
    tx.locktime = 0;

    // Set up the output:
    AddressMeta addressMeta;
    wallet.addresses.getNew(addressMeta);
    bc::transaction_output_type output;
    ABC_CHECK(outputScriptForAddress(output.script, addressMeta.address));
    tx.outputs.push_back(output);

    // Set up the inputs:
    uint64_t fee, funds;
    ABC_CHECK(inputsPickMaximum(fee, funds, tx, utxos));
    if (outputIsDust(funds))
        return ABC_ERROR(ABC_CC_InsufficientFunds, "Not enough funds");
    tx.outputs[0].value = funds;

    // Now sign that:
    KeyTable keys;
    keys[address] = wif;
    ABC_CHECK(signTx(tx, wallet.cache.txs, keys));

    // Send:
    bc::data_chunk raw_tx(satoshi_raw_size(tx));
    bc::satoshi_save(tx, raw_tx.begin());
    ABC_CHECK(broadcastTx(wallet, raw_tx));

    // Calculate transaction information:
    const auto info = wallet.cache.txs.txInfo(tx);
    const auto balance = wallet.addresses.balance(info);

    // Save the transaction metadata:
    TxMeta meta;
    meta.ntxid = info.ntxid;
    meta.txid = info.txid;
    meta.timeCreation = time(nullptr);
    meta.internal = true;
    meta.airbitzFeeWanted = 0;
    meta.airbitzFeeSent = 0;
    ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                  meta.metadata.amountCurrency, balance,
                  static_cast<Currency>(wallet.currency())));
    ABC_CHECK(wallet.txs.save(meta, balance, info.fee));

    // Update the transaction cache:
    if (wallet.cache.txs.insert(tx))
        wallet.cache.save().log(); // Failure is fine
    wallet.balanceDirty();
    ABC_CHECK(wallet.addresses.markOutputs(info));

    // Done:
    ABC_DebugLog("IncomingSweep callback: wallet %s, txid: %s, value: %d",
                 wallet.id().c_str(), info.txid.c_str(), balance);
    tABC_AsyncBitCoinInfo async;
    async.pData = pData;
    async.eventType = ABC_AsyncEventType_IncomingSweep;
    Status().toError(async.status, ABC_HERE());
    async.szWalletUUID = wallet.id().c_str();
    async.szTxID = info.txid.c_str();
    async.sweepSatoshi = balance;
    fCallback(&async);

    return Status();
}

void
sweepOnComplete(Wallet &wallet,
                const std::string &address, const std::string &wif,
                tABC_BitCoin_Event_Callback fCallback, void *pData)
{
    auto s = sweepSend(wallet, address, wif, fCallback, pData).log();
    if (!s)
    {
        ABC_DebugLog("IncomingSweep callback: wallet %s, status: %d",
                     wallet.id().c_str(), s.value());
        tABC_AsyncBitCoinInfo info;
        info.pData = pData;
        info.eventType = ABC_AsyncEventType_IncomingSweep;
        s.toError(info.status, ABC_HERE());
        info.szWalletUUID = wallet.id().c_str();
        info.szTxID = nullptr;
        info.sweepSatoshi = 0;
        fCallback(&info);
    }
}

} // namespace abcd
