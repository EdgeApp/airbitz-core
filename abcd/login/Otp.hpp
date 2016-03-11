/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file Two-factor authentication support.
 */

#ifndef ABCD_LOGIN_OTP_HPP
#define ABCD_LOGIN_OTP_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <list>

namespace abcd {

class Lobby;
class Login;

/**
 * Reads the OTP configuration from the server.
 */
Status
otpAuthGet(Login &login, bool &enabled, long &timeout);

/**
 * Sets up OTP authentication on the server.
 *
 * @param lobby The lobby contains the username and OTP key.
 * If the lobby has no OTP key, this function will create one.
 * @param timeout Reset time, in seconds.
 */
Status
otpAuthSet(Login &login, long timeout);

/**
 * Removes the OTP authentication requirement from the server.
 */
Status
otpAuthRemove(Login &login);

/**
 * Returns the reset status for a group of accounts.
 */
Status
otpResetGet(std::list<std::string> &result,
            const std::list<std::string> &usernames);

/**
 * Launches an OTP reset timer on the server,
 * which will disable the OTP authentication requirement when it expires.
 */
Status
otpResetSet(Lobby &lobby, const std::string &token);

/**
 * Cancels an OTP reset timer.
 */
Status
otpResetRemove(Login &login);

} // namespace abcd

#endif
