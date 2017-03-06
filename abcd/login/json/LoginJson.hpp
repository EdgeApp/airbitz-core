/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_SERVER_LOGIN_JSON_HPP
#define ABCD_LOGIN_SERVER_LOGIN_JSON_HPP

#include "../../json/JsonArray.hpp"
#include "../../json/JsonBox.hpp"
#include "../../json/JsonSnrp.hpp"

namespace abcd {

class Login;
class LoginStashJson;

/**
 * Login information returned by the server.
 */
class LoginReplyJson:
    public JsonObject
{
public:
    ABC_JSON_CONSTRUCTORS(LoginReplyJson, JsonObject)

    // Identity:
    ABC_JSON_STRING(appId, "appId", "")
    ABC_JSON_STRING(loginId, "loginId", nullptr)
    ABC_JSON_VALUE(loginAuthBox, "loginAuthBox", JsonBox)
    ABC_JSON_VALUE(children, "children", JsonArray)

    // Parent:
    ABC_JSON_VALUE(parentBox, "parentBox", JsonBox)

    // Password:
    ABC_JSON_VALUE(passwordAuthBox, "passwordAuthBox", JsonBox)
    ABC_JSON_VALUE(passwordBox, "passwordBox", JsonBox)
    ABC_JSON_VALUE(passwordKeySnrp, "passwordKeySnrp", JsonSnrp)

    // PIN v2:
    ABC_JSON_VALUE(pin2Box, "pin2Box", JsonBox)
    ABC_JSON_VALUE(pin2KeyBox, "pin2KeyBox", JsonBox)

    // Recovery v1:
    ABC_JSON_VALUE(questionBox, "questionBox", JsonBox)
    ABC_JSON_VALUE(questionKeySnrp, "questionKeySnrp", JsonSnrp)
    ABC_JSON_VALUE(recoveryBox, "recoveryBox", JsonBox)
    ABC_JSON_VALUE(recoveryKeySnrp, "recoveryKeySnrp", JsonSnrp)

    // Recovery v2:
    ABC_JSON_VALUE(question2Box, "question2Box", JsonBox)
    ABC_JSON_VALUE(recovery2Box, "recovery2Box", JsonBox)
    ABC_JSON_VALUE(recovery2KeyBox, "recovery2KeyBox", JsonBox)

    // Keys:
    ABC_JSON_VALUE(mnemonicBox, "mnemonicBox", JsonBox)
    ABC_JSON_VALUE(rootKeyBox, "rootKeyBox", JsonBox)
    // dataKeyBox
    ABC_JSON_VALUE(syncKeyBox, "syncKeyBox", JsonBox)
    ABC_JSON_VALUE(keyBoxes, "keyBoxes", JsonArray)

    /**
     * Breaks out the fields and writes them to disk.
     */
    Status
    save(const Login &login);

    /**
     * Filters the server reply down to the on-disk storage format.
     */
    Status
    makeLoginStashJson(LoginStashJson &result, DataSlice dataKey,
                       const std::string &username);
};

/**
 * Login information saved to disk (new format).
 */
class LoginStashJson:
    public JsonObject
{
public:
    ABC_JSON_CONSTRUCTORS(LoginStashJson, JsonObject)

    ABC_JSON_STRING(username, "username", nullptr)

    // Identity:
    ABC_JSON_STRING(appId, "appId", "")
    ABC_JSON_STRING(loginId, "loginId", nullptr)
    ABC_JSON_VALUE(loginAuthBox, "loginAuthBox", JsonBox)
    ABC_JSON_VALUE(children, "children", JsonArray)

    // Parent:
    ABC_JSON_VALUE(parentBox, "parentBox", JsonBox)

    // Password:
    ABC_JSON_VALUE(passwordBox, "passwordBox", JsonBox)
    ABC_JSON_VALUE(passwordKeySnrp, "passwordKeySnrp", JsonSnrp)
    ABC_JSON_VALUE(passwordAuthBox, "passwordAuthBox", JsonBox)

    // PIN v2:
    ABC_JSON_STRING(pin2Key, "pin2Key", nullptr)

    // Recovery v1:
    ABC_JSON_VALUE(questionBox, "questionBox", JsonBox)
    ABC_JSON_VALUE(questionKeySnrp, "questionKeySnrp", JsonSnrp)
    ABC_JSON_VALUE(recoveryBox, "recoveryBox", JsonBox)
    ABC_JSON_VALUE(recoveryKeySnrp, "recoveryKeySnrp", JsonSnrp)

    // Recovery v2:
    ABC_JSON_STRING(recovery2Key, "recovery2Key", nullptr)

    // Keys:
    ABC_JSON_VALUE(mnemonicBox, "mnemonicBox", JsonBox)
    ABC_JSON_VALUE(rootKeyBox, "rootKeyBox", JsonBox)
    ABC_JSON_VALUE(syncKeyBox, "syncKeyBox", JsonBox)
    ABC_JSON_VALUE(keyBoxes, "keyBoxes", JsonArray)

    /**
     * Prunes a the tree down to just the items needed for a specific app.
     * Returns a null JSON if this whole branch is irrlelevant.
     */
    Status
    makeEdgeLogin(LoginStashJson &result, const std::string &appId);

    /**
     * Finds the loginKey for the particular appId.
     */
    Status
    findLoginKey(DataChunk &result, DataSlice dataKey, const std::string &appId);
};

} // namespace abcd

#endif
