/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Account.hpp"
#include "AccountSettings.hpp"
#include "../login/Login.hpp"
#include "../util/Sync.hpp"
#include "../util/AutoFree.hpp"

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
    ABC_CHECK(syncRepo(dir(), login.syncKey(), dirty));
    if (dirty)
        ABC_CHECK(load());

    return Status();
}

Account::Account(Login &login):
    login(login),
    parent_(login.shared_from_this()),
    dir_(login.paths.syncDir()),
    wallets(*this)
{}

Status
Account::load()
{
    // If the sync dir doesn't exist, create it:
    const auto tempPath = login.paths.dir() + "tmp/";
    ABC_CHECK(syncEnsureRepo(dir(), tempPath, login.syncKey()));

    AutoFree<tABC_AccountSettings, accountSettingsFree> settings;
    settings.get() = accountSettingsLoad(*this);
    bool pinChanged = false; // TODO: settings->szPIN != last-login-pin?
    ABC_CHECK(accountSettingsPinSync(login, settings, pinChanged));

    ABC_CHECK(wallets.load());
    return Status();
}

} // namespace abcd
