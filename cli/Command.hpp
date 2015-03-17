/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef CLI_COMMAND_HPP
#define CLI_COMMAND_HPP

#include "../abcd/util/Status.hpp"
#include <memory>

namespace abcd {
class Lobby;
class Login;
class Account;
}

/**
 * Different levels of information that can be populated in the session.
 */
enum class InitLevel
{
    none = 0,   // Core not initialized
    context,    // Core initialized, but nothing loaded
    lobby,      // Username, but no login
    login,      // Fully logged-in user
    account,    // Fully logged-in user with synced data
    wallet      // Full login plus wallet id
};

/**
 * The main function fills this in as needed,
 * giving the commands access to the objects they need.
 */
struct Session
{
    std::shared_ptr<abcd::Lobby> lobby;
    std::shared_ptr<abcd::Login> login;
    std::shared_ptr<abcd::Account> account;
    std::string uuid; // Wallet
};

/**
 * The function prototype for commands.
 */
#define COMMAND_PROTO \
    operator ()(Session &session, int argc, char *argv[])

/**
 * Abstract base class for commands.
 */
class Command
{
public:
    virtual ~Command();
    virtual abcd::Status COMMAND_PROTO = 0;
    virtual InitLevel level() = 0;
    virtual const char *name() = 0;
};

/**
 * Inserts a new command in to the global command list.
 */
class CommandRegistry
{
public:
    CommandRegistry(const char *name, Command *c);

    /**
     * Finds a command in the global list.
     */
    static Command *
    find(const std::string &name);

    /**
     * Prints the list of commands.
     */
    static void
    print();
};

/**
 * Registers and defines new command.
 * Should be followed by the command implementation in curly braces.
 */
#define COMMAND(LEVEL, NAME, TEXT) \
    class NAME: public Command { \
        abcd::Status COMMAND_PROTO override; \
        InitLevel level() override { return LEVEL; } \
        const char *name() override { return TEXT; } \
    } implement##NAME; \
    CommandRegistry register##NAME(TEXT, &implement##NAME); \
    abcd::Status NAME::COMMAND_PROTO

#endif
