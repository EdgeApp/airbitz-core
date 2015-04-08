/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Account.hpp"
#include "../login/Login.hpp"
#include "../util/Sync.hpp"

namespace abcd {

Account::Account(std::shared_ptr<Login> login):
    login_(login),
    dir_(login->syncDir()),
    wallets(*this)
{}

Status
Account::init()
{
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
