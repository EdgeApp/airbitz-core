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
class LoginStore;

/**
 * A proof of a user's identity for the login server.
 */
class AuthJson:
    public JsonObject
{
public:
    ABC_JSON_CONSTRUCTORS(AuthJson, JsonObject)

    Status
    otpSet(const LoginStore &store);

    Status
    userIdSet(const LoginStore &store);

    Status
    passwordSet(const LoginStore &store, DataSlice passwordAuth);

    Status
    recoverySet(const LoginStore &store, DataSlice recoveryAuth);

    Status
    recovery2Set(const LoginStore &store, DataSlice recovery2Id);

    Status
    recovery2Set(const LoginStore &store, DataSlice recovery2Id,
                 JsonPtr recovery2Auth);

    Status
    loginSet(const Login &login);

protected:
    ABC_JSON_STRING(otp, "otp", nullptr)
    ABC_JSON_STRING(userId, "userId", nullptr)
    ABC_JSON_STRING(passwordAuth, "passwordAuth", nullptr)
    ABC_JSON_STRING(recoveryAuth, "recoveryAuth", nullptr)
    ABC_JSON_VALUE(recovery2Auth, "recovery2Auth", JsonPtr)
    ABC_JSON_STRING(recovery2Id, "recovery2Id", nullptr)
};

} // namespace abcd

#endif
