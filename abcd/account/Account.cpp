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

Account::Account(std::shared_ptr<Login> login):
    login_(login),
    dir_(login->lobby().dir() + "sync/"),
    wallets(*this)
{}

Status
Account::init()
{
    // Locate the sync dir:
    bool exists = false;
    ABC_CHECK_OLD(ABC_FileIOFileExists(dir().c_str(), &exists, &error));

    // If it doesn't exist, create it:
    if (!exists)
    {
        bool dirty = false;
        std::string tempName = login().lobby().dir() + "tmp/";
        ABC_CHECK_OLD(ABC_FileIOFileExists(tempName.c_str(), &exists, &error));
        if (exists)
            ABC_CHECK_OLD(ABC_FileIODeleteRecursive(tempName.c_str(), &error));
        ABC_CHECK_OLD(ABC_SyncMakeRepo(tempName.c_str(), &error));
        ABC_CHECK_OLD(ABC_SyncRepo(tempName.c_str(), login().syncKey().c_str(), dirty, &error));
        if (rename(tempName.c_str(), dir().c_str()))
            return ABC_ERROR(ABC_CC_SysError, "rename failed");
    }

    ABC_CHECK(wallets.load());
    return Status();
}

Status
Account::sync(bool &dirty)
{
    ABC_CHECK_OLD(ABC_SyncRepo(dir().c_str(), login().syncKey().c_str(), dirty, &error));
    if (dirty)
    {
        ABC_CHECK(wallets.load());
    }

    return Status();
}

} // namespace abcd
