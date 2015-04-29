/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Command.hpp"
#include "../abcd/account/PluginData.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::account, PluginGet, "plugin-get")
{
    if (argc != 4)
        return ABC_ERROR(ABC_CC_Error, "usage: ... plugin-get <user> <pass> <plugin> <key>");
    std::string value;
    ABC_CHECK(pluginDataGet(*session.account, argv[2], argv[3], value));
    std::cout << '"' << value << '"' << std::endl;
    return Status();
}

COMMAND(InitLevel::account, PluginSet, "plugin-set")
{
    if (argc != 5)
        return ABC_ERROR(ABC_CC_Error, "usage: ... plugin-set <user> <pass> <plugin> <key> <value>");
    ABC_CHECK(pluginDataSet(*session.account, argv[2], argv[3], argv[4]));
    return Status();
}

COMMAND(InitLevel::account, PluginRemove, "plugin-remove")
{
    if (argc != 4)
        return ABC_ERROR(ABC_CC_Error, "usage: ... plugin-remove <user> <pass> <plugin> <key>");
    ABC_CHECK(pluginDataRemove(*session.account, argv[2], argv[3]));
    return Status();
}

COMMAND(InitLevel::account, PluginClear, "plugin-clear")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... plugin-clear <user> <pass> <plugin>");
    ABC_CHECK(pluginDataClear(*session.account, argv[2]));
    return Status();
}
