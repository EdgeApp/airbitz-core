/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * PIN-based re-login logic.
 */

#ifndef ABCD_LOGIN_LOGIN_PIN_HPP
#define ABCD_LOGIN_LOGIN_PIN_HPP

#include "../util/Status.hpp"
#include <time.h>
#include <memory>

namespace abcd {

class Login;
class Lobby;

/**
 * Determines whether or not the given user can log in via PIN on this
 * device.
 */
Status
loginPinExists(bool &result, const std::string &username);

/**
 * Deletes the local copy of the PIN-based login data.
 */
Status
loginPinDelete(const Lobby &lobby);

/**
 * Assuming a PIN-based login pagage exits, log the user in.
 */
Status
loginPin(std::shared_ptr<Login> &result,
         Lobby &lobby, const std::string &pin);

/**
 * Sets up a PIN login package, both on-disk and on the server.
 */
Status
loginPinSetup(Login &login, const std::string &pin, time_t expires);

} // namespace abcd

#endif
