/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Wallet.hpp"
#include "../Context.hpp"
#include "../Tx.hpp"
#include "../account/Account.hpp"
#include "../bitcoin/WatcherBridge.hpp"
#include "../crypto/Encoding.hpp"
#include "../json/JsonObject.hpp"
#include <assert.h>

namespace abcd {

struct WalletJson:
    public JsonObject
{
    ABC_JSON_STRING(bitcoinKey, "BitcoinSeed", nullptr)
    ABC_JSON_STRING(dataKey,    "MK",          nullptr)
    ABC_JSON_STRING(syncKey,    "SyncKey",     nullptr)
};

Status
Wallet::create(std::shared_ptr<Wallet> &result, Account &account,
    const std::string &id)
{
    std::shared_ptr<Wallet> out(new Wallet(account, id));
    ABC_CHECK(out->loadKeys());

    result = std::move(out);
    return Status();
}

const DataChunk &
Wallet::bitcoinKey() const
{
    // We do not want memory corruption here.
    // Otherwise, we might generate a bad bitcoin address and lose money:
    assert(bitcoinKeyBackup_ == bitcoinKey_);
    return bitcoinKey_;
}

Status
Wallet::balance(int64_t &result)
{
    // We cannot put a mutex in `balanceDirty()`, since that will deadlock
    // the transaction database during the balance calculation.
    // Instead, we access `balanceDirty_` atomically outside the mutex:
    bool dirty = balanceDirty_;
    balanceDirty_ = false;

    std::lock_guard<std::mutex> lock(mutex_);
    if (dirty)
    {
        tABC_TxInfo **aTransactions = nullptr;
        unsigned int nTxCount = 0;

        ABC_CHECK_OLD(ABC_TxGetTransactions(*this,
            ABC_GET_TX_ALL_TIMES, ABC_GET_TX_ALL_TIMES,
            &aTransactions, &nTxCount, &error));
        ABC_CHECK_OLD(ABC_BridgeFilterTransactions(*this,
            aTransactions, &nTxCount, &error));

        balance_ = 0;
        for (unsigned i = 0; i < nTxCount; ++i)
            balance_ += aTransactions[i]->pDetails->amountSatoshi;

        // TODO: Leaks if ABC_BridgeFilterTransactions fails.
        ABC_TxFreeTransactions(aTransactions, nTxCount);
    }

    result = balance_;
    return Status();
}

void
Wallet::balanceDirty()
{
    balanceDirty_ = true;
}

Wallet::Wallet(Account &account, const std::string &id):
    account(account),
    parent_(account.shared_from_this()),
    id_(id),
    dir_(gContext->walletsDir() + id + "/"),
    balanceDirty_(true)
{}

Status
Wallet::loadKeys()
{
    WalletJson json;
    ABC_CHECK(account.wallets.json(json, id()));
    ABC_CHECK(json.bitcoinKeyOk());
    ABC_CHECK(json.dataKeyOk());
    ABC_CHECK(json.syncKeyOk());

    ABC_CHECK(base16Decode(bitcoinKey_, json.bitcoinKey()));
    bitcoinKeyBackup_ = bitcoinKey_;
    ABC_CHECK(base16Decode(dataKey_, json.dataKey()));
    syncKey_ = json.syncKey();

    return Status();
}

} // namespace abcd
