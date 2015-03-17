/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Command.hpp"
#include "../src/LoginShim.hpp"
#include <iostream>

using namespace abcd;

#define CA_CERT "./cli/ca-certificates.crt"

/**
 * The main program body.
 */
static Status run(int argc, char *argv[])
{
    if (argc < 2)
    {
        CommandRegistry::print();
        return Status();
    }

    // Find the command:
    Command *command = CommandRegistry::find(argv[1]);
    if (!command)
        return ABC_ERROR(ABC_CC_Error, "unknown command " + std::string(argv[1]));

    // Populate the session up to the required level:
    Session session;
    if (InitLevel::context <= command->level())
    {
        if (argc < 3)
            return ABC_ERROR(ABC_CC_Error, std::string("No working directory given"));

        unsigned char seed[] = {1, 2, 3};
        ABC_CHECK_OLD(ABC_Initialize(argv[2], CA_CERT, seed, sizeof(seed), &error));
    }
    if (InitLevel::lobby <= command->level())
    {
        if (argc < 4)
            return ABC_ERROR(ABC_CC_Error, std::string("No username given"));

        ABC_CHECK(cacheLobby(session.lobby, argv[3]));
    }
    if (InitLevel::login <= command->level())
    {
        if (argc < 5)
            return ABC_ERROR(ABC_CC_Error, std::string("No password given"));

        ABC_CHECK_OLD(ABC_SignIn(argv[3], argv[4], &error));
        ABC_CHECK(cacheLogin(session.login, argv[3]));
    }
    if (InitLevel::account <= command->level())
    {
        ABC_CHECK(cacheAccount(session.account, argv[3]));
    }
    if (InitLevel::wallet <= command->level())
    {
        if (argc < 6)
            return ABC_ERROR(ABC_CC_Error, std::string("No wallet name given"));

        session.uuid = argv[5];
    }

    // Invoke the command:
    ABC_CHECK((*command)(session, argc-3, argv+3));

    // Clean up:
    ABC_Terminate();
    return Status();
}

int main(int argc, char *argv[])
{
    Status s = run(argc, argv);
    if (!s)
        std::cerr << s << std::endl;
    return s ? 0 : 1;
}
