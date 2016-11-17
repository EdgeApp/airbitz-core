/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/login/server/LoginServer.hpp"
#include "../../src/LoginShim.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::store, PinLogin, "pin-login",
        " <pin>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto pin = argv[0];

    bool exists;
    ABC_CHECK_OLD(ABC_PinLoginExists(session.username.c_str(),
                                     &exists, &error));
    if (!exists)
        return ABC_ERROR(ABC_CC_PinExpired, "PIN login is not present");

    AuthError authError;
    auto s = cacheLoginPin(session.login,
                           session.username.c_str(),
                           pin,
                           authError);
    if (authError.pinWait)
        std::cout << "Please try again in " << authError.pinWait
                  << " seconds" << std::endl;
    ABC_CHECK(s);

    return Status();
}

COMMAND(InitLevel::account, PinLoginSetup, "pin-login-setup",
        " <pin>")
{
    if (1 != argc)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto pin = argv[0];

    ABC_CHECK_OLD(ABC_PinSetup(session.username.c_str(),
                               session.password.c_str(),
                               pin, &error));

    return Status();
}
