/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Storage backend for login data.
 */

#ifndef ABCD_LOGIN_LOGIN_DIR_HPP
#define ABCD_LOGIN_LOGIN_DIR_HPP

#include "../util/Status.hpp"
#include <list>
#include <string>

namespace abcd {

/**
 * List all the accounts currently on the device.
 */
std::list<std::string>
loginDirList();

/**
 * Locates the account directory for a given username.
 * Returns a blank string if there is no directory.
 */
std::string
loginDirFind(const std::string &username);

/**
 * If the login directory does not exist, create it.
 * This is meant to be called after `loginDirFind`,
 * and will do nothing if the given directory is already populated.
 */
Status
loginDirCreate(std::string &directory, const std::string &username);

} // namespace abcd

#endif
