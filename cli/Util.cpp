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

Status syncAll(const Login &login)
{
    // Sync the account:
    ABC_CHECK_OLD(ABC_DataSyncAccount(login.lobby().username().c_str(),
        nullptr, nullptr, nullptr, &error));

    // Sync the wallets:
    AutoStringArray uuids;
    ABC_CHECK_OLD(ABC_AccountWalletList(login, &uuids.data, &uuids.size, &error));
    for (size_t i = 0; i < uuids.size; ++i)
    {
        int dirty = 0;
        ABC_CHECK_OLD(ABC_WalletSyncData(ABC_WalletID(login, uuids.data[i]), &dirty, &error));
    }

    return Status();
}
