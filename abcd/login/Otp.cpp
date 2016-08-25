/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Otp.hpp"
#include "Login.hpp"
#include "LoginStore.hpp"
#include "server/LoginServer.hpp"

namespace abcd {

Status
otpAuthGet(Login &login, bool &enabled, long &timeout)
{
    return loginServerOtpStatus(login, enabled, timeout);
}

Status
otpAuthSet(Login &login, long timeout)
{
    // Install a key if needed:
    if (!login.store.otpKey())
    {
        OtpKey random;
        ABC_CHECK(random.create());
        login.store.otpKeySet(random);
    }

    ABC_CHECK(loginServerOtpEnable(login,
                                   login.store.otpKey()->encodeBase32(), timeout));

    return Status();
}

Status
otpAuthRemove(Login &login)
{
    return loginServerOtpDisable(login);
}

Status
otpResetGet(std::list<std::string> &result,
            const std::list<std::string> &usernames)
{
    // List the users:
    std::list<DataChunk> userIds;
    for (const auto &i: usernames)
    {
        std::shared_ptr<LoginStore> store;
        ABC_CHECK(LoginStore::create(store, i));
        auto userId = store->userId();
        userIds.emplace_back(userId.begin(), userId.end());
    }

    // Make the request:
    std::list<bool> flags;
    ABC_CHECK(loginServerOtpPending(userIds, flags));

    // Smush the results:
    result.clear();
    auto i = flags.begin();
    auto j = usernames.begin();
    while (i != flags.end() && j != usernames.end())
    {
        if (*i)
            result.push_back(*j);
        ++i;
        ++j;
    }

    return Status();
}

Status
otpResetSet(LoginStore &store, const std::string &token)
{
    return loginServerOtpReset(store, token);
}

Status
otpResetRemove(Login &login)
{
    return loginServerOtpResetCancelPending(login);
}

} // namespace abcd
