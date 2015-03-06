/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef CLI_PLUGIN_DATA_HPP
#define CLI_PLUGIN_DATA_HPP

#include "../src/ABC.h"
#include "../abcd/util/Status.hpp"

abcd::Status pluginGet(int argc, char *argv[]);
abcd::Status pluginSet(int argc, char *argv[]);
abcd::Status pluginRemove(int argc, char *argv[]);
abcd::Status pluginClear(int argc, char *argv[]);

#endif
