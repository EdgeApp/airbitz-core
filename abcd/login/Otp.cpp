/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Otp.hpp"
#include "Lobby.hpp"
#include "Login.hpp"
#include "../auth/LoginServer.hpp"

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
    if (!login.lobby.otpKey())
    {
        OtpKey random;
        ABC_CHECK(random.create());
        login.lobby.otpKeySet(random);
    }

    ABC_CHECK(loginServerOtpEnable(login,
        login.lobby.otpKey()->encodeBase32(), timeout));

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
    std::list<DataChunk> authIds;
    for (const auto &i: usernames)
    {
        std::shared_ptr<Lobby> lobby;
        ABC_CHECK(Lobby::create(lobby, i));
        auto authId = lobby->authId();
        authIds.emplace_back(authId.begin(), authId.end());
    }

    // Make the request:
    std::list<bool> flags;
    ABC_CHECK(loginServerOtpPending(authIds, flags));

    // Smush the results:
    result.clear();
    auto i = flags.begin();
    auto j = usernames.begin();
    while (i != flags.end() && j != usernames.end())
    {
        if (*i)
            result.push_back(*j);
        ++i; ++j;
    }

    return Status();
}

Status
otpResetSet(Lobby &lobby)
{
    return loginServerOtpReset(lobby);
}

Status
otpResetRemove(Login &login)
{
    return loginServerOtpResetCancelPending(login);
}

} // namespace abcd
