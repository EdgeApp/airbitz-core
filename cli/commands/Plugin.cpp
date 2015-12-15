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

COMMAND(InitLevel::account, PluginGet, "plugin-get",
        " <plugin> <key>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto plugin = argv[0];
    const auto key = argv[1];

    std::string value;
    ABC_CHECK(pluginDataGet(*session.account, plugin, key, value));
    std::cout << '"' << value << '"' << std::endl;
    return Status();
}

COMMAND(InitLevel::account, PluginSet, "plugin-set",
        " <plugin> <key> <value>")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto plugin = argv[0];
    const auto key = argv[1];
    const auto value = argv[2];

    ABC_CHECK(pluginDataSet(*session.account, plugin, key, value));
    return Status();
}

COMMAND(InitLevel::account, PluginRemove, "plugin-remove",
        " <plugin> <key>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto plugin = argv[0];
    const auto key = argv[1];

    ABC_CHECK(pluginDataRemove(*session.account, plugin, key));
    return Status();
}

COMMAND(InitLevel::account, PluginClear, "plugin-clear",
        " <plugin>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto plugin = argv[0];

    ABC_CHECK(pluginDataClear(*session.account, plugin));
    return Status();
}
