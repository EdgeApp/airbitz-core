/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Command.hpp"
#include "../abcd/json/JsonObject.hpp"
#include "../abcd/util/Util.hpp"
#include "../src/LoginShim.hpp"
#include <iostream>
#include <getopt.h>

using namespace abcd;

#define CA_CERT "./cli/ca-certificates.crt"

struct ConfigJson:
    public JsonObject
{
    ABC_JSON_STRING(apiKey, "apiKey", nullptr)
    ABC_JSON_STRING(hiddenBitzKey, "hiddenBitzKey", nullptr)
    ABC_JSON_STRING(workingDir, "workingDir", nullptr)
    ABC_JSON_STRING(username, "username", nullptr)
    ABC_JSON_STRING(password, "password", nullptr)
    ABC_JSON_STRING(wallet, "wallet", nullptr)
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
    ABC_CHECK(json.hiddenBitzKeyOk());

    // Parse out the command-line options:
    std::string workingDir;
    Session session;
    bool wantHelp = false;

    static const struct option long_options[] =
    {
        {"working-dir", required_argument, nullptr, 'd'},
        {"username",    required_argument, nullptr, 'u'},
        {"password",    required_argument, nullptr, 'p'},
        {"wallet",      required_argument, nullptr, 'w'},
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };
    opterr = 0;
    int c;
    while (-1 != (c = getopt_long(argc, argv, "d:hu:p:w:", long_options, nullptr)))
    {
        switch (c)
        {
        case 'd':
            workingDir = optarg;
            break;
        case 'h':
            wantHelp = true;
            break;
        case 'p':
            session.password = optarg;
            break;
        case 'u':
            session.username = optarg;
            break;
        case 'w':
            session.uuid = optarg;
            break;
        case '?':
            if (optopt == 'd')
                return ABC_ERROR(ABC_CC_Error, std::string("-d requires a working directory"));
            else if (optopt == 'p')
                return ABC_ERROR(ABC_CC_Error, std::string("-p requires a password"));
            else if (optopt == 'u')
                return ABC_ERROR(ABC_CC_Error, std::string("-u requires a username"));
            else if (optopt == 'w')
                return ABC_ERROR(ABC_CC_Error, std::string("-w requires a wallet id"));
            else
                return ABC_ERROR(ABC_CC_Error, std::string("Unknown option '-%c'.", optopt));
        default:
            abort();
        }
    }

    // At this point, all non-option arguments should be out of the list:
    argc -= optind;
    argv += optind;

    // Find the command:
    if (argc < 1)
    {
        CommandRegistry::print();
        return Status();
    }
    const auto commandName = argv[0];
    --argc;
    ++argv;

    Command *command = CommandRegistry::find(commandName);
    if (!command)
        return ABC_ERROR(ABC_CC_Error,
                         "unknown command " + std::string(commandName));

    // If the user wants help, just print the string and return:
    if (wantHelp)
    {
        std::cout << helpString(*command) << std::endl;
        return Status();
    }

    // Populate the session up to the required level:
    if (InitLevel::context <= command->level())
    {
        if (workingDir.empty())
        {
            if (json.workingDirOk())
                workingDir = json.workingDir();
            else
                return ABC_ERROR(ABC_CC_Error, "No working directory given, " +
                                 helpString(*command));
        }

        unsigned char seed[] = {1, 2, 3};
        ABC_CHECK_OLD(ABC_Initialize(workingDir.c_str(),
                                     CA_CERT,
                                     json.apiKey(),
                                     json.hiddenBitzKey(),
                                     seed,
                                     sizeof(seed),
                                     &error));
    }
    if (InitLevel::lobby <= command->level())
    {
        if (session.username.empty())
        {
            if (json.usernameOk())
                session.username = json.username();
            else
                return ABC_ERROR(ABC_CC_Error, "No username given, " +
                                 helpString(*command));
        }

        ABC_CHECK(cacheLobby(session.lobby, session.username.c_str()));
    }
    if (InitLevel::login <= command->level())
    {
        if (session.password.empty())
        {
            if (json.passwordOk())
                session.password = json.password();
            else
                return ABC_ERROR(ABC_CC_Error, "No password given, " +
                                 helpString(*command));
        }

        auto s = cacheLoginPassword(session.login,
                                    session.username.c_str(),
                                    session.password.c_str());
        if (ABC_CC_InvalidOTP == s.value())
        {
            AutoString date;
            ABC_CHECK_OLD(ABC_OtpResetDate(&date.get(), &error));
            if (strlen(date))
                std::cout << "Pending OTP reset ends at " << date.get() << std::endl;
            std::cout << "No OTP token, resetting account 2-factor auth." << std::endl;
            ABC_CHECK_OLD(ABC_OtpResetSet(session.username.c_str(), &error));
        }
        ABC_CHECK(s);
    }
    if (InitLevel::account <= command->level())
    {
        ABC_CHECK(cacheAccount(session.account, session.username.c_str()));
    }
    if (InitLevel::wallet <= command->level())
    {
        if (session.uuid.empty())
        {
            if (json.walletOk())
                session.uuid = json.wallet();
            else
                return ABC_ERROR(ABC_CC_Error, "No wallet name given, " +
                                 helpString(*command));
        }

        ABC_CHECK(cacheWallet(session.wallet,
                              session.username.c_str(), session.uuid.c_str()));
    }

    // Invoke the command:
    ABC_CHECK((*command)(session, argc, argv));

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
