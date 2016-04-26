/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Receive.hpp"
#include "Wallet.hpp"
#include "../Context.hpp"
#include "../General.hpp"
#include "../bitcoin/cache/Cache.hpp"
#include "../exchange/ExchangeCache.hpp"
#include "../spend/AirbitzFee.hpp"
#include "../util/Debug.hpp"

namespace abcd {

Status
onReceive(Wallet &wallet, const TxInfo &info,
          tABC_BitCoin_Event_Callback fCallback, void *pData)
{
    wallet.balanceDirty();
    ABC_CHECK(wallet.addresses.markOutputs(info));

    // Does the transaction already exist?
    TxMeta meta;
    if (!wallet.txs.get(meta, info.ntxid))
    {
        const auto balance = wallet.addresses.balance(info);

        meta.ntxid = info.ntxid;
        meta.txid = info.txid;
        meta.timeCreation = time(nullptr);
        meta.internal = false;
        meta.airbitzFeeWanted = 0;
        meta.airbitzFeeSent = 0;

        // Receives can accumulate Airbitz fees:
        const auto airbitzFeeInfo = generalAirbitzFeeInfo();
        meta.airbitzFeeWanted = airbitzFeeIncoming(airbitzFeeInfo, balance);
        logInfo("Airbitz fee: " +
                std::to_string(meta.airbitzFeeWanted) + " wanted, " +
                std::to_string(wallet.txs.airbitzFeePending()) + " pending");

        // Grab metadata from the address:
        for (const auto &io: info.ios)
        {
            AddressMeta address;
            if (wallet.addresses.get(address, io.address))
                meta.metadata = address.metadata;
        }
        ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                      meta.metadata.amountCurrency, balance,
                      static_cast<Currency>(wallet.currency())));

        // Save the metadata:
        ABC_CHECK(wallet.txs.save(meta, balance, info.fee));

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
        fCallback(&async);
    }
    else
    {
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
        fCallback(&async);
    }

    return Status();
}

} // namespace abcd
