/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Util.hpp"
#include "../abcd/Wallet.hpp"
#include "../abcd/account/Account.hpp"
#include "../abcd/login/Lobby.hpp"
#include "../abcd/login/Login.hpp"

using namespace abcd;

Status syncAll(Account &account)
{
    // Sync the account:
    bool dirty = false;
    ABC_CHECK(account.sync(dirty));

    // Sync the wallets:
    auto uuids = account.wallets.list();
    for (const auto &i: uuids)
    {
        auto wallet = ABC_WalletID(account, i.id.c_str());
        ABC_CHECK_OLD(ABC_WalletSyncData(wallet, dirty, &error));
    }

    return Status();
}
