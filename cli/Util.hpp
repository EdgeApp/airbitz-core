/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Utilities and helpers shared between commands.
 */

#ifndef CLI_UTIL_HPP
#define CLI_UTIL_HPP

#include "../abcd/util/Status.hpp"

namespace abcd {
    class Account;
}

/**
 * Syncs the
 */
abcd::Status
syncAll(abcd::Account &account);

#endif
