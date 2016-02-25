/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Tx.hpp"
#include "Context.hpp"
#include "bitcoin/TxDatabase.hpp"
#include "spend/Spend.hpp"
#include "util/Debug.hpp"
#include "wallet/Wallet.hpp"

namespace abcd {

static Status   txSaveNewTx(Wallet &self, Tx &tx,
                            const std::vector<std::string> &addresses, bool bOutside);

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
txSendSave(Wallet &self,
           const std::string &ntxid, const std::string &txid,
           const std::vector<std::string> &addresses, SendInfo *pInfo)
{
    // set the state
    Tx tx;
    tx.ntxid = ntxid;
    tx.txid = txid;
    tx.timeCreation = time(nullptr);
    tx.internal = true;
    tx.metadata = pInfo->metadata;

    // Add in tx fees to the amount of the tx
    if (self.addresses.has(pInfo->destAddress))
    {
        tx.metadata.amountSatoshi = pInfo->metadata.amountFeesAirbitzSatoshi
                                    + pInfo->metadata.amountFeesMinersSatoshi;
    }
    else
    {
        tx.metadata.amountSatoshi = pInfo->metadata.amountSatoshi
                                    + pInfo->metadata.amountFeesAirbitzSatoshi
                                    + pInfo->metadata.amountFeesMinersSatoshi;
    }

    ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                  tx.metadata.amountCurrency, tx.metadata.amountSatoshi,
                  static_cast<Currency>(self.currency())));

    if (tx.metadata.amountSatoshi > 0)
        tx.metadata.amountSatoshi *= -1;
    if (tx.metadata.amountCurrency > 0)
        tx.metadata.amountCurrency *= -1.0;

    // Save the transaction:
    ABC_CHECK(txSaveNewTx(self, tx, addresses, false));

    if (pInfo->bTransfer)
    {
        Tx receiveTx;
        receiveTx.ntxid = ntxid;
        receiveTx.txid = txid;
        receiveTx.timeCreation = time(nullptr);
        receiveTx.internal = true;
        receiveTx.metadata = pInfo->metadata;

        // Set the payee name:
        receiveTx.metadata.name = self.name();

        //
        // Since this wallet is receiving, it didn't really get charged AB fees
        // This should really be an assert since no transfers should have AB fees
        //
        receiveTx.metadata.amountFeesAirbitzSatoshi = 0;

        ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                      receiveTx.metadata.amountCurrency, receiveTx.metadata.amountSatoshi,
                      static_cast<Currency>(self.currency())));

        if (receiveTx.metadata.amountSatoshi < 0)
            receiveTx.metadata.amountSatoshi *= -1;
        if (receiveTx.metadata.amountCurrency < 0)
            receiveTx.metadata.amountCurrency *= -1.0;

        // save the transaction
        ABC_CHECK(txSaveNewTx(*pInfo->walletDest, receiveTx, addresses, false));
    }

    return Status();
}

Status
txReceiveTransaction(Wallet &self,
                     const std::string &ntxid, const std::string &txid,
                     const std::vector<std::string> &addresses,
                     tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    Tx temp;

    // Does the transaction already exist?
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

        // add the transaction to the address
        ABC_CHECK(txSaveNewTx(self, tx, addresses, true));

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

/**
 * Saves the a never-before-seen transaction to the sync database,
 * updating the metadata as appropriate.
 *
 * @param bOutside true if this is an outside transaction that needs its
 * details populated from the address database.
 */
Status
txSaveNewTx(Wallet &self, Tx &tx,
            const std::vector<std::string> &addresses, bool bOutside)
{
    // Mark addresses as used:
    TxMetadata metadata;
    for (const auto &i: addresses)
    {
        Address address;
        if (self.addresses.get(address, i))
        {
            // Update the transaction:
            if (address.recyclable)
            {
                address.recyclable = false;
                ABC_CHECK(self.addresses.save(address));
            }
            metadata = address.metadata;
        }
    }

    // Copy the metadata (if any):
    if (bOutside)
    {
        if (tx.metadata.name.empty() && !metadata.name.empty())
            tx.metadata.name = metadata.name;
        if (tx.metadata.notes.empty() && !metadata.notes.empty())
            tx.metadata.notes = metadata.notes;
        if (tx.metadata.category.empty() && !metadata.category.empty())
            tx.metadata.category = metadata.category;
        tx.metadata.bizId = metadata.bizId;
    }
    ABC_CHECK(self.txs.save(tx));
    self.balanceDirty();

    return Status();
}

} // namespace abcd
