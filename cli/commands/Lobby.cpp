/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/login/AccountRequest.hpp"
#include "../../abcd/login/server/LoginServer.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::context, LobbyGet, "lobby-get",
        " <id>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    auto id = argv[0];

    JsonPtr lobbyJson;
    ABC_CHECK(loginServerLobbyGet(lobbyJson, id));

    std::cout << "Contents:" << std::endl;
    AccountRequest request;
    if (accountRequest(request, lobbyJson))
    {
        std::cout << "  Account request:" << std::endl;
        std::cout << "    Name:\t" << request.displayName << std::endl;
        std::cout << "    Name:\t" << request.displayImageUrl << std::endl;
        std::cout << "    Type:\t" << request.type << std::endl;
    }

    return Status();
}

COMMAND(InitLevel::login, LobbyApproveEdge, "lobby-approve-edge",
        " <id>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    auto id = argv[0];

    JsonPtr lobbyJson;
    ABC_CHECK(loginServerLobbyGet(lobbyJson, id));
    ABC_CHECK(accountRequestApprove(*session.login, id, "", lobbyJson));

    return Status();
}
