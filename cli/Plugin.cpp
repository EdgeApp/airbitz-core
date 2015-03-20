/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Command.hpp"
#include "../abcd/util/Util.hpp"
#include "../src/ABC.h"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::account, PluginGet, "plugin-get")
{
    if (argc != 4)
        return ABC_ERROR(ABC_CC_Error, "usage: ... plugin-get <user> <pass> <plugin> <key>");
    AutoString result;
    ABC_CHECK_OLD(ABC_PluginDataGet(argv[0], argv[1], argv[2], argv[3], &result.get(), &error));
    std::cout << '"' << result.get() << '"' << std::endl;
    return Status();
}

COMMAND(InitLevel::account, PluginSet, "plugin-set")
{
    if (argc != 5)
        return ABC_ERROR(ABC_CC_Error, "usage: ... plugin-set <user> <pass> <plugin> <key> <value>");
    ABC_CHECK_OLD(ABC_PluginDataSet(argv[0], argv[1], argv[2], argv[3], argv[4], &error));
    return Status();
}

COMMAND(InitLevel::account, PluginRemove, "plugin-remove")
{
    if (argc != 4)
        return ABC_ERROR(ABC_CC_Error, "usage: ... plugin-remove <user> <pass> <plugin> <key>");
    ABC_CHECK_OLD(ABC_PluginDataRemove(argv[0], argv[1], argv[2], argv[3], &error));
    return Status();
}

COMMAND(InitLevel::account, PluginClear, "plugin-clear")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... plugin-clear <user> <pass> <plugin>");
    ABC_CHECK_OLD(ABC_PluginDataClear(argv[0], argv[1], argv[2], &error));
    return Status();
}
