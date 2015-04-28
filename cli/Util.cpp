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
    AutoStringArray uuids;
    ABC_CHECK_OLD(ABC_AccountWalletList(account.login(), &uuids.data, &uuids.size, &error));
    for (size_t i = 0; i < uuids.size; ++i)
    {
        auto wallet = ABC_WalletID(account.login(), uuids.data[i]);
        ABC_CHECK_OLD(ABC_WalletSyncData(wallet, dirty, &error));
    }

    return Status();
}
