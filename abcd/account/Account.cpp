/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Account.hpp"
#include "../Context.hpp"
#include "../crypto/Encoding.hpp"
#include "../login/Login.hpp"
#include "../login/json/KeyJson.hpp"
#include "../util/Sync.hpp"

namespace abcd {

Status
Account::create(std::shared_ptr<Account> &result, Login &login)
{
    AccountRepoJson repoJson;
    ABC_CHECK(login.repoFind(repoJson, gContext->accountType(), true));
    DataChunk dataKey;
    DataChunk syncKey;
    ABC_CHECK(base64Decode(dataKey, repoJson.dataKey()));
    ABC_CHECK(base64Decode(syncKey, repoJson.syncKey()));
    std::shared_ptr<Account> out(new Account(login, dataKey, syncKey));
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

Account::Account(Login &login, DataSlice dataKey, DataSlice syncKey):
    login(login),
    parent_(login.shared_from_this()),
    dir_(login.paths.syncDir()),
    dataKey_(dataKey.begin(), dataKey.end()),
    syncKey_(base16Encode(syncKey)),
    wallets(*this)
{}

Status
Account::load()
{
    // If the sync dir doesn't exist, create it:
    const auto tempPath = login.paths.dir() + "tmp/";
    ABC_CHECK(syncEnsureRepo(dir(), tempPath, syncKey_));

    ABC_CHECK(wallets.load());
    return Status();
}

} // namespace abcd
