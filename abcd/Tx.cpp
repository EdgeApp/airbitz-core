/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Tx.hpp"
#include "Context.hpp"
#include "bitcoin/TxDatabase.hpp"
#include "util/Debug.hpp"
#include "wallet/Wallet.hpp"

namespace abcd {

Status
txSweepSave(Wallet &self,
            const std::string &ntxid, const std::string &txid,
            uint64_t funds)
{
    Tx tx;
    tx.ntxid = ntxid;
    tx.txid = txid;
    tx.timeCreation = time(nullptr);
    tx.internal = true;
    tx.metadata.amountSatoshi = funds;
    tx.metadata.amountFeesAirbitzSatoshi = 0;
    ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                  tx.metadata.amountCurrency, tx.metadata.amountSatoshi,
                  static_cast<Currency>(self.currency())));

    // save the transaction
    ABC_CHECK(self.txs.save(tx));
    self.balanceDirty();

    return Status();
}

Status
txReceiveTransaction(Wallet &self,
                     const std::string &ntxid, const std::string &txid,
                     const std::vector<std::string> &addresses,
                     tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    // Does the transaction already exist?
    Tx temp;
    if (!self.txs.get(temp, ntxid))
    {
        Tx tx;
        tx.ntxid = ntxid;
        tx.txid = txid;
        tx.timeCreation = time(nullptr);
        tx.internal = false;
        ABC_CHECK(self.txdb.ntxidAmounts(ntxid, self.addresses.list(),
                                         tx.metadata.amountSatoshi,
                                         tx.metadata.amountFeesMinersSatoshi));
        ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                      tx.metadata.amountCurrency, tx.metadata.amountSatoshi,
                      static_cast<Currency>(self.currency())));

        // Grab metadata from the address:
        TxMetadata metadata;
        for (const auto &i: addresses)
        {
            Address address;
            if (self.addresses.get(address, i))
                tx.metadata = address.metadata;
        }

        // Save the metadata:
        ABC_CHECK(self.txs.save(tx));
        self.balanceDirty();

        // Update the GUI:
        ABC_DebugLog("IncomingBitCoin callback: wallet %s, txid: %s, ntxid: %s",
                     self.id().c_str(), txid.c_str(), ntxid.c_str());
        tABC_AsyncBitCoinInfo info;
        info.pData = pData;
        info.eventType = ABC_AsyncEventType_IncomingBitCoin;
        Status().toError(info.status, ABC_HERE());
        info.szWalletUUID = self.id().c_str();
        info.szTxID = ntxid.c_str();
        info.sweepSatoshi = 0;
        fAsyncCallback(&info);
    }
    else
    {
        // Mark the wallet cache as dirty in case the Tx wasn't included in the current balance
        self.balanceDirty();

        // Update the GUI:
        ABC_DebugLog("BalanceUpdate callback: wallet %s, txid: %s, ntxid: %s",
                     self.id().c_str(), txid.c_str(), ntxid.c_str());
        tABC_AsyncBitCoinInfo info;
        info.pData = pData;
        info.eventType = ABC_AsyncEventType_BalanceUpdate;
        Status().toError(info.status, ABC_HERE());
        info.szWalletUUID = self.id().c_str();
        info.szTxID = ntxid.c_str();
        info.sweepSatoshi = 0;
        fAsyncCallback(&info);
    }

    return Status();
}

} // namespace abcd
