/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/http/Uri.hpp"
#include "../../abcd/login/Bitid.hpp"
#include "../../abcd/login/Login.hpp"

using namespace abcd;

COMMAND(InitLevel::login, BitidLogin, "bitid-login")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... spend-uri <user> <pass> <uri>");
    const char *uri = argv[2];

    Uri callback;
    ABC_CHECK(bitidCallback(callback, uri, false));
    callback.pathSet("");
    std::cout << "Signing in to " << callback.encode() << std::endl;

    bitidLogin(session.login->rootKey(), uri, 0);

    return Status();
}
