/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Wallet.hpp"
#include "../Context.hpp"
#include "../Tx.hpp"
#include "../Wallet.hpp"
#include "../account/Account.hpp"
#include "../bitcoin/WatcherBridge.hpp"
#include "../crypto/Encoding.hpp"
#include "../crypto/Random.hpp"
#include "../json/JsonObject.hpp"
#include "../login/Login.hpp"
#include "../login/LoginServer.hpp"
#include "../util/FileIO.hpp"
#include "../util/Sync.hpp"
#include <assert.h>

namespace abcd {

#define WALLET_CURRENCY_FILENAME "Currency.json"

struct WalletJson:
    public JsonObject
{
    ABC_JSON_STRING(bitcoinKey, "BitcoinSeed", nullptr)
    ABC_JSON_STRING(dataKey,    "MK",          nullptr)
    ABC_JSON_STRING(syncKey,    "SyncKey",     nullptr)
};

struct CurrencyJson:
    public JsonObject
{
    ABC_JSON_INTEGER(currency, "num", 0)
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

Status
Wallet::createNew(std::shared_ptr<Wallet> &result, Account &account,
    const std::string &name, int currency)
{
    std::string id;
    ABC_CHECK(randomUuid(id));
    std::shared_ptr<Wallet> out(new Wallet(account, id));
    ABC_CHECK(out->createNew(name, currency));

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
Wallet::createNew(const std::string &name, int currency)
{
    // Set up the keys:
    ABC_CHECK(randomData(bitcoinKey_, BITCOIN_SEED_LENGTH));
    bitcoinKeyBackup_ = bitcoinKey_;
    ABC_CHECK(randomData(dataKey_, DATA_KEY_LENGTH));
    DataChunk syncKey;
    ABC_CHECK(randomData(syncKey, SYNC_KEY_LENGTH));
    syncKey_ = base16Encode(syncKey);

    // Create the sync directory:
    ABC_CHECK(fileEnsureDir(gContext->walletsDir()));
    ABC_CHECK(fileEnsureDir(dir()));
    ABC_CHECK(fileEnsureDir(syncDir()));
    ABC_CHECK(syncMakeRepo(syncDir()));

    // Populate the sync directory:
    CurrencyJson currencyJson;
    ABC_CHECK(currencyJson.currencySet(currency));
    ABC_CHECK(currencyJson.save(syncDir() + WALLET_CURRENCY_FILENAME, dataKey()));
    ABC_CHECK_OLD(ABC_WalletSetName(*this, name.c_str(), &error));
    ABC_CHECK_OLD(ABC_TxCreateInitialAddresses(*this, &error));

    // Push the wallet to the server:
    bool dirty = false;
    AutoU08Buf LP1;
    ABC_CHECK_OLD(ABC_LoginGetServerKey(account.login, &LP1, &error));
    ABC_CHECK(LoginServerWalletCreate(account.login.lobby, LP1, syncKey_.c_str()));
    ABC_CHECK(syncRepo(syncDir(), syncKey_, dirty));
    ABC_CHECK(LoginServerWalletActivate(account.login.lobby, LP1, syncKey_.c_str()));

    // If everything worked, add the wallet to the account:
    WalletJson json;
    ABC_CHECK(json.bitcoinKeySet(base16Encode(bitcoinKey_)));
    ABC_CHECK(json.dataKeySet(base16Encode(dataKey_)));
    ABC_CHECK(json.syncKeySet(syncKey_));
    ABC_CHECK(account.wallets.insert(id_, json));
    ABC_CHECK(account.sync(dirty));

    return Status();
}

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
