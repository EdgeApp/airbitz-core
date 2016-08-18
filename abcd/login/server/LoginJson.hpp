/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_SERVER_LOGIN_JSON_HPP
#define ABCD_LOGIN_SERVER_LOGIN_JSON_HPP

#include "../../json/JsonBox.hpp"
#include "../../json/JsonSnrp.hpp"

namespace abcd {

class AccountPaths;

/**
 * Login information returned by the server.
 */
class LoginJson:
    public JsonObject
{
public:
    ABC_JSON_CONSTRUCTORS(LoginJson, JsonObject)

    // Password login:
    ABC_JSON_VALUE(passwordBox, "passwordBox", JsonBox)
    ABC_JSON_VALUE(passwordKeySnrp, "passwordKeySnrp", JsonSnrp)

    // Recovery v1:
    ABC_JSON_VALUE(questionBox, "questionBox", JsonBox)
    ABC_JSON_VALUE(questionKeySnrp, "questionKeySnrp", JsonSnrp)
    ABC_JSON_VALUE(recoveryBox, "recoveryBox", JsonBox)
    ABC_JSON_VALUE(recoveryKeySnrp, "recoveryKeySnrp", JsonSnrp)

    // Recovery v2:
    ABC_JSON_VALUE(question2Box, "question2Box", JsonBox)
    ABC_JSON_VALUE(recovery2Box, "recovery2Box", JsonBox)

    // Keys:
    ABC_JSON_VALUE(passwordAuthBox, "passwordAuthBox", JsonBox)
    ABC_JSON_VALUE(recovery2KeyBox, "recovery2KeyBox", JsonBox)
    ABC_JSON_VALUE(rootKeyBox, "rootKeyBox", JsonBox)
    // dataKeyBox
    // mnemonicBox
    // repos
    ABC_JSON_VALUE(syncKeyBox, "syncKeyBox", JsonBox)

    /**
     * Breaks out the fields and writes them to disk.
     */
    Status
    save(const AccountPaths &paths, DataSlice dataKey);
};

} // namespace abcd

#endif
