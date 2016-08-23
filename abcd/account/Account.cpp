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
    RepoInfo repoInfo;
    ABC_CHECK(login.repoFind(repoInfo, repoTypeAirbitzAccount, true));
    std::shared_ptr<Account> out(new Account(login,
                                 repoInfo.dataKey,
                                 repoInfo.syncKey));
    ABC_CHECK(out->load());

    result = std::move(out);
    return Status();
}

Status
Account::sync(bool &dirty)
{
    ABC_CHECK(syncRepo(dir(), syncKey_, dirty));
    if (dirty)
        ABC_CHECK(load());

    return Status();
}

Account::Account(Login &login, DataSlice dataKey, const std::string &syncKey):
    login(login),
    parent_(login.shared_from_this()),
    dir_(login.paths.syncDir()),
    dataKey_(dataKey.begin(), dataKey.end()),
    syncKey_(syncKey),
    wallets(*this)
{}

Status
Account::load()
{
    // If the sync dir doesn't exist, create it:
    const auto tempPath = login.paths.dir() + "tmp/";
    ABC_CHECK(syncEnsureRepo(dir(), tempPath, syncKey_));

    AutoFree<tABC_AccountSettings, accountSettingsFree> settings;
    settings.get() = accountSettingsLoad(*this);
    bool pinChanged = false; // TODO: settings->szPIN != last-login-pin?
    ABC_CHECK(accountSettingsPinSync(login, settings, pinChanged));

    ABC_CHECK(wallets.load());
    return Status();
}

} // namespace abcd
