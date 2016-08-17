/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Recovery-question v2 login logic.
 */

#ifndef ABCD_LOGIN_LOGIN_RECOVERY_2_HPP
#define ABCD_LOGIN_LOGIN_RECOVERY_2_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <list>
#include <memory>

namespace abcd {

class Login;
class LoginStore;
struct AuthError;

/**
 * Obtains the recovery questions for a user.
 */
Status
loginRecovery2Questions(std::list<std::string> &result,
                        LoginStore &store, DataSlice recovery2Key);

/**
 * Creates a login object using recovery answers.
 */
Status
loginRecovery2(std::shared_ptr<Login> &result,
               LoginStore &store, DataSlice recovery2Key,
               const std::list<std::string> &answers,
               AuthError &authError);

/**
 * Changes the recovery questions and answers on an existing login object.
 */
Status
loginRecovery2Set(DataChunk &result, Login &login,
                  const std::list<std::string> &questions,
                  const std::list<std::string> &answers);

} // namespace abcd

#endif
