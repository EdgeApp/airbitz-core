/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Lobby.hpp"
#include "Login.hpp"
#include "LoginDir.hpp"

namespace abcd {

Status
Lobby::init(const std::string &username)
{
    // Set up identity:
    AutoString szUsername;
    ABC_CHECK_OLD(ABC_LoginFixUserName(username.c_str(), &szUsername.get(), &error));
    username_ = szUsername.get();
    directory_ = loginDirFind(username_);

    // Create authId:
    // TODO: Make this lazy!
    AutoFree<tABC_CryptoSNRP, ABC_CryptoFreeSNRP> SNRP0;
    ABC_CHECK_OLD(ABC_CryptoCreateSNRPForServer(&SNRP0.get(), &error));
    AutoU08Buf L1;
    ABC_CHECK_OLD(ABC_CryptoScryptSNRP(toU08Buf(username_), SNRP0, &L1, &error));
    authId_ = DataChunk(L1.p, L1.end);

    return Status();
}

Status
Lobby::createDirectory()
{
    ABC_CHECK_OLD(ABC_LoginDirCreate(directory_, username_.c_str(), &error));
    return Status();
}

} // namespace abcd
