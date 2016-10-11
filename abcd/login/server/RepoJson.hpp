/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_SERVER_REPO_JSON_HPP
#define ABCD_LOGIN_SERVER_REPO_JSON_HPP

#include "../../json/JsonBox.hpp"

namespace abcd {

constexpr auto repoTypeAirbitzAccount = "account:repo:co.airbitz.wallet";

/**
 * Information about a repository attached to a login (decoded).
 */
struct RepoInfo
{
    std::string type;
    DataChunk dataKey;
    std::string syncKey;
};

/**
 * General information about a repository attached to a login.
 * This information is visible to the login server.
 */
class RepoJson:
    public JsonObject
{
public:
    ABC_JSON_CONSTRUCTORS(RepoJson, JsonObject)
    ABC_JSON_VALUE(infoBox, "info", JsonBox)
    ABC_JSON_STRING(type, "type", nullptr)

    /**
     * Decodes and decrypts this JSON into a `RepoInfo` structure.
     */
    Status
    decode(RepoInfo &result, DataSlice dataKey);

    /**
     * Builds and encrypts a `RepoJson` object.
     */
    Status
    encode(const RepoInfo &info, DataSlice dataKey);
};

/**
 * Keys and other details needed to open a specific repository.
 * This information is encrypted.
 */
class RepoInfoJson:
    public JsonObject
{
public:
    ABC_JSON_CONSTRUCTORS(RepoInfoJson, JsonObject);
    ABC_JSON_STRING(syncKey, "syncKey", "!bad") // base16
    ABC_JSON_STRING(dataKey, "dataKey", "!bad") // base16
};

} // namespace abcd

#endif
