/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Password-based login logic.
 */

#ifndef ABCD_LOGIN_LOGIN_PASSWORD_HPP
#define ABCD_LOGIN_LOGIN_PASSWORD_HPP

#include "../util/Status.hpp"
#include <memory>

namespace abcd {

class Login;
class Lobby;

/**
 * Loads an existing login object, either from the server or from disk.
 */
Status
loginPassword(std::shared_ptr<Login> &result,
              Lobby &lobby,
              const std::string &password);

/**
 * Changes the password on an existing login object.
 */
Status
loginPasswordSet(Login &login, const std::string &password);

/**
 * Validates that the provided password is correct.
 * This is used in the GUI to guard access to certain actions.
 */
Status
loginPasswordOk(bool &result, Login &login, const std::string &password);

/**
 * Returns true if the logged-in account has a password.
 */
Status
loginPasswordExists(bool &result, const Lobby &lobby);

} // namespace abcd

#endif
