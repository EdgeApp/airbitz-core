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

#include "LoginPackages.hpp"
#include "../../src/ABC.h"
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

tABC_CC ABC_LoginDirCreate(std::string &directory,
                           const char *szUserName,
                           tABC_Error *pError);

} // namespace abcd

#endif
