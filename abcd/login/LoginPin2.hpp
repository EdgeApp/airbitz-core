/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * PIN v2 login logic.
 */

#ifndef ABCD_LOGIN_LOGIN_PIN_2_HPP
#define ABCD_LOGIN_LOGIN_PIN_2_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <list>
#include <memory>

namespace abcd {

class AccountPaths;
class Login;
class LoginStore;
struct AuthError;

/**
 * Loads the pin2Key from disk, if present.
 */
Status
loginPin2Key(DataChunk &result, const AccountPaths &paths);

/**
 * Stashes a pin2Key on disk for future reference.
 */
Status
loginPin2KeySave(DataSlice pin2Key, const AccountPaths &paths);

/**
 * Creates a login object using the PJN.
 */
Status
loginPin2(std::shared_ptr<Login> &result,
          LoginStore &store, DataSlice pin2Key,
          const std::string &pin,
          AuthError &authError);

/**
 * Changes the PIN on an existing login object.
 */
Status
loginPin2Set(DataChunk &result, Login &login,
             const std::string &pin);

/**
 * Removes the PIN from the given login.
 */
Status
loginPin2Delete(Login &login);

} // namespace abcd

#endif
