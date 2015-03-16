/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Otp.hpp"
#include "Lobby.hpp"
#include "Login.hpp"
#include "LoginServer.hpp"

namespace abcd {

Status
otpAuthGet(Login &login, bool &enabled, long &timeout)
{
    AutoU08Buf L1;
    AutoU08Buf LP1;
    ABC_CHECK_OLD(ABC_LoginGetServerKeys(login, &L1, &LP1, &error));
    ABC_CHECK_OLD(ABC_LoginServerOtpStatus(L1, LP1, &enabled, &timeout, &error));

    return Status();
}

Status
otpAuthSet(Login &login, long timeout)
{
    // Install a key if needed:
    if (!login.lobby().otpKey())
    {
        OtpKey random;
        ABC_CHECK(random.create());
        login.lobby().otpKeySet(random);
    }

    AutoU08Buf L1;
    AutoU08Buf LP1;
    ABC_CHECK_OLD(ABC_LoginGetServerKeys(login, &L1, &LP1, &error));
    ABC_CHECK_OLD(ABC_LoginServerOtpEnable(L1, LP1,
        login.lobby().otpKey()->encodeBase32().c_str(), timeout, &error));

    return Status();
}

Status
otpAuthRemove(Login &login)
{
    AutoU08Buf L1;
    AutoU08Buf LP1;
    ABC_CHECK_OLD(ABC_LoginGetServerKeys(login, &L1, &LP1, &error));
    ABC_CHECK_OLD(ABC_LoginServerOtpDisable(L1, LP1, &error));

    return Status();
}

Status
otpResetGet(std::list<DataChunk> authIds, std::list<bool> &result)
{
    ABC_CHECK_OLD(ABC_LoginServerOtpPending(authIds, result, &error));
    return Status();
}

Status
otpResetSet(Lobby &lobby)
{
    ABC_CHECK_OLD(ABC_LoginServerOtpReset(toU08Buf(lobby.authId()), &error));

    return Status();
}

Status
otpResetRemove(Login &login)
{
    AutoU08Buf L1;
    AutoU08Buf LP1;
    ABC_CHECK_OLD(ABC_LoginGetServerKeys(login, &L1, &LP1, &error));
    ABC_CHECK_OLD(ABC_LoginServerOtpResetCancelPending(L1, LP1, &error));

    return Status();
}

} // namespace abcd
