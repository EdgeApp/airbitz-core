/*
 * Copyright (c) 2014, Airbitz, Inc.
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
class LoginStore;
struct AuthError;

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
loginPinDelete(LoginStore &store);

/**
 * Assuming a PIN-based login pagage exits, log the user in.
 */
Status
loginPin(std::shared_ptr<Login> &result,
         LoginStore &store, const std::string &pin,
         AuthError &authError);

} // namespace abcd

#endif
