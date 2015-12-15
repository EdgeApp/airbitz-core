/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/account/PluginData.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::account, PluginGet, "plugin-get")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... plugin-get <user> <pass> <plugin> <key>");
    const auto plugin = argv[0];
    const auto key = argv[1];

    std::string value;
    ABC_CHECK(pluginDataGet(*session.account, plugin, key, value));
    std::cout << '"' << value << '"' << std::endl;
    return Status();
}

COMMAND(InitLevel::account, PluginSet, "plugin-set")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... plugin-set <user> <pass> <plugin> <key> <value>");
    const auto plugin = argv[0];
    const auto key = argv[1];
    const auto value = argv[2];

    ABC_CHECK(pluginDataSet(*session.account, plugin, key, value));
    return Status();
}

COMMAND(InitLevel::account, PluginRemove, "plugin-remove")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... plugin-remove <user> <pass> <plugin> <key>");
    const auto plugin = argv[0];
    const auto key = argv[1];

    ABC_CHECK(pluginDataRemove(*session.account, plugin, key));
    return Status();
}

COMMAND(InitLevel::account, PluginClear, "plugin-clear")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... plugin-clear <user> <pass> <plugin>");
    const auto plugin = argv[0];

    ABC_CHECK(pluginDataClear(*session.account, plugin));
    return Status();
}
