/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * A means of authenticating with an Airbitz auth server.
 */

#ifndef ABCD_LOGIN_SERVER_AUTH_JSON_HPP
#define ABCD_LOGIN_SERVER_AUTH_JSON_HPP

#include "../../json/JsonObject.hpp"

namespace abcd {

class Login;

/**
 * A proof of a user's identity for the login server.
 */
class AuthJson:
    public JsonObject
{
public:
    ABC_JSON_CONSTRUCTORS(AuthJson, JsonObject)

    Status
    loginSet(const Login &login);

protected:
    ABC_JSON_STRING(otp, "otp", nullptr)
    ABC_JSON_STRING(userId, "userId", nullptr)
    ABC_JSON_STRING(passwordAuth, "passwordAuth", nullptr)
};

} // namespace abcd

#endif
