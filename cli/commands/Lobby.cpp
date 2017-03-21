/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/login/Sharing.hpp"
#include "../../abcd/util/Util.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::context, LobbyGet, "lobby-get",
        " <id>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    auto id = argv[0];

    Lobby lobby;
    ABC_CHECK(lobbyFetch(lobby, id));

    std::cout << "Contents:" << std::endl;
    LoginRequest request;
    if (loginRequestLoad(request, lobby))
    {
        std::cout << "  Account request:" << std::endl;
        std::cout << "    appId:\t" << request.appId << std::endl;
        std::cout << "    name:\t" << request.displayName << std::endl;
        std::cout << "    image:\t" << request.displayImageUrl << std::endl;
    }
    else
    {
        std::cout << "  " << lobby.json.encode() << std::endl;
    }

    return Status();
}

COMMAND(InitLevel::account, LobbyApproveEdge, "lobby-approve-edge",
        " <id>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    auto id = argv[0];

    // Get PIN:
    AutoFree<tABC_AccountSettings, ABC_FreeAccountSettings> pSettings;
    ABC_CHECK_OLD(ABC_LoadAccountSettings(session.username.c_str(),
                                          session.password.c_str(),
                                          &pSettings.get(), &error));
    auto pin = pSettings->szPIN ? pSettings->szPIN : "";

    Lobby lobby;
    ABC_CHECK(lobbyFetch(lobby, id));
    ABC_CHECK(loginRequestApprove(*session.login, lobby, pin));

    return Status();
}
