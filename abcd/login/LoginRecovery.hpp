/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Recovery-question login logic.
 */

#ifndef ABCD_LOGIN_LOGIN_RECOVERY_HPP
#define ABCD_LOGIN_LOGIN_RECOVERY_HPP

#include "../util/Status.hpp"
#include <memory>

namespace abcd {

class Login;
class Lobby;

/**
 * Obtains the recovery questions for a user.
 */
Status
loginRecoveryQuestions(std::string &result, Lobby &lobby);

/**
 * Creates a login object using recovery answers rather than a password.
 */
Status
loginRecovery(std::shared_ptr<Login> &result,
              Lobby &lobby, const std::string &recoveryAnswers);

/**
 * Changes the recovery questions and answers on an existing login object.
 */
Status
loginRecoverySet(Login &login,
                 const std::string &recoveryQuestions,
                 const std::string &recoveryAnswers);

} // namespace abcd

#endif
