/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_SERVER_KEY_JSON_HPP
#define ABCD_LOGIN_SERVER_KEY_JSON_HPP

#include "../../json/JsonBox.hpp"

namespace abcd {

constexpr auto repoTypeAirbitzAccount = "account:repo:co.airbitz.wallet";
constexpr auto keyIdLength = 32;

/**
 * Information about an asset's keys.
 */
struct KeyJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(KeyJson, JsonObject)

    ABC_JSON_STRING(id, "id", nullptr) // base64
    ABC_JSON_STRING(type, "type", nullptr)
    ABC_JSON_VALUE(keys, "keys", JsonPtr)
};

/**
 * Keys for an account settings repo.
 */
struct AccountRepoJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(AccountRepoJson, JsonObject)

    ABC_JSON_STRING(syncKey, "syncKey", "!bad") // base64
    ABC_JSON_STRING(dataKey, "dataKey", "!bad") // base64
};

} // namespace abcd

#endif
