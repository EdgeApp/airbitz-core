/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Command.hpp"
#include "../abcd/json/JsonObject.hpp"
#include "../src/LoginShim.hpp"
#include <iostream>

using namespace abcd;

#define CA_CERT "./cli/ca-certificates.crt"

struct ConfigJson:
    public JsonObject
{
    ABC_JSON_STRING(apiKey, "apiKey", nullptr)
    ABC_JSON_STRING(chainKey, "chainKey", nullptr)
    ABC_JSON_STRING(hiddenBitzKey, "hiddenBitzKey", nullptr)
};

static std::string
configPath()
{
    // Mac: ~/Library/Application Support/Airbitz/airbitz.conf
    // Unix: ~/.config/airbitz/airbitz.conf
    const char *home = getenv("HOME");
    if (!home || !strlen(home))
        home = "/";

#ifdef MAC_OSX
    return std::string(home) + "/Library/Application Support/Airbitz/airbitz.conf";
#else
    return std::string(home) + "/.config/airbitz/airbitz.conf";
#endif
}

/**
 * The main program body.
 */
static Status run(int argc, char *argv[])
{
    ConfigJson json;
    ABC_CHECK(json.load(configPath()));
    ABC_CHECK(json.apiKeyOk());
    ABC_CHECK(json.chainKeyOk());
    ABC_CHECK(json.hiddenBitzKeyOk());

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
        ABC_CHECK_OLD(ABC_Initialize(argv[2],
                                     CA_CERT,
                                     json.apiKey(),
                                     json.chainKey(),
                                     json.hiddenBitzKey(),
                                     seed,
                                     sizeof(seed),
                                     &error));
    }
    if (InitLevel::lobby <= command->level())
    {
        if (argc < 4)
            return ABC_ERROR(ABC_CC_Error, std::string("No username given"));

        session.username = argv[3];
        ABC_CHECK(cacheLobby(session.lobby, session.username));
    }
    if (InitLevel::login <= command->level())
    {
        if (argc < 5)
            return ABC_ERROR(ABC_CC_Error, std::string("No password given"));

        session.password = argv[4];
        ABC_CHECK_OLD(ABC_SignIn(session.username, session.password, &error));
        ABC_CHECK(cacheLogin(session.login, session.username));
    }
    if (InitLevel::account <= command->level())
    {
        ABC_CHECK(cacheAccount(session.account, session.username));
    }
    if (InitLevel::wallet <= command->level())
    {
        if (argc < 6)
            return ABC_ERROR(ABC_CC_Error, std::string("No wallet name given"));

        session.uuid = argv[5];
        ABC_CHECK(cacheWallet(session.wallet, session.username, session.uuid));
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
