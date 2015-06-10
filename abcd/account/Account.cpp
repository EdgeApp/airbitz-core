/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Account.hpp"
#include "../login/Lobby.hpp"
#include "../login/Login.hpp"
#include "../util/FileIO.hpp"
#include "../util/Sync.hpp"

namespace abcd {

Status
Account::create(std::shared_ptr<Account> &result, Login &login)
{
    std::shared_ptr<Account> out(new Account(login));
    ABC_CHECK(out->load());

    result = std::move(out);
    return Status();
}

Status
Account::sync(bool &dirty)
{
    ABC_CHECK_OLD(ABC_SyncRepo(dir().c_str(), login.syncKey().c_str(), dirty, &error));
    if (dirty)
        ABC_CHECK(load());

    return Status();
}

Account::Account(Login &login):
    login(login),
    parent_(login.shared_from_this()),
    dir_(login.lobby.dir() + "sync/"),
    wallets(*this)
{}

Status
Account::load()
{
    // If the sync dir doesn't exist, create it:
    if (!fileExists(dir()))
    {
        bool dirty = false;
        std::string tempName = login.lobby.dir() + "tmp/";
        if (fileExists(tempName))
            ABC_CHECK_OLD(ABC_FileIODeleteRecursive(tempName.c_str(), &error));
        ABC_CHECK_OLD(ABC_SyncMakeRepo(tempName.c_str(), &error));
        ABC_CHECK_OLD(ABC_SyncRepo(tempName.c_str(), login.syncKey().c_str(), dirty, &error));
        if (rename(tempName.c_str(), dir().c_str()))
            return ABC_ERROR(ABC_CC_SysError, "rename failed");
    }

    ABC_CHECK(wallets.load());
    return Status();
}

} // namespace abcd
