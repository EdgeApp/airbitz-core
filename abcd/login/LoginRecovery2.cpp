/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginRecovery2.hpp"
#include "Login.hpp"
#include "LoginStore.hpp"
#include "server/AuthJson.hpp"
#include "server/LoginJson.hpp"
#include "server/LoginServer.hpp"
#include "../crypto/Crypto.hpp"
#include "../crypto/Encoding.hpp"
#include "../crypto/Random.hpp"
#include "../json/JsonArray.hpp"
#include "../json/JsonBox.hpp"

namespace abcd {

struct Recovery2KeyJson:
    public JsonObject
{
    ABC_JSON_STRING(recovery2Key, "recovery2Key", "!bad")
};

/**
 * Builds the recover2Auth JSON array.
 */
static Status
recovery2AuthBuild(JsonPtr &result, DataSlice recovery2Key,
                   const std::list<std::string> &answers)
{
    JsonArray arrayJson;
    for (const auto &answer: answers)
    {
        const auto s = base64Encode(hmacSha256(answer, recovery2Key));
        ABC_CHECK(arrayJson.append(json_string(s.c_str())));
    }

    result = arrayJson;
    return Status();
}

Status
loginRecovery2Key(DataChunk &result, const AccountPaths &paths)
{
    Recovery2KeyJson json;
    ABC_CHECK(json.load(paths.recovery2KeyPath()));
    ABC_CHECK(base58Decode(result, json.recovery2Key()));

    return Status();
}

Status
loginRecovery2KeySave(DataSlice recovery2Key, const AccountPaths &paths)
{
    Recovery2KeyJson json;
    ABC_CHECK(json.recovery2KeySet(base58Encode(recovery2Key)));
    ABC_CHECK(json.save(paths.recovery2KeyPath()));

    return Status();
}

Status
loginRecovery2Questions(std::list<std::string> &result,
                        LoginStore &store, DataSlice recovery2Key)
{
    const auto recovery2Id = hmacSha256(store.username(), recovery2Key);

    // Grab the login information from the server:
    AuthJson authJson;
    LoginJson loginJson;
    ABC_CHECK(authJson.recovery2Set(store, recovery2Id));
    ABC_CHECK(loginServerLogin(loginJson, authJson));

    // Decrypt:
    DataChunk questions;
    JsonArray arrayJson;
    ABC_CHECK(loginJson.question2Box().decrypt(questions, recovery2Key));
    ABC_CHECK(arrayJson.decode(toString(questions)));

    // Unpack:
    std::list<std::string> out;
    size_t size = arrayJson.size();
    for (size_t i = 0; i < size; i++)
    {
        auto stringJson = arrayJson[i];
        if (!json_is_string(stringJson.get()))
            return ABC_ERROR(ABC_CC_JSONError, "Question is not a string");

        out.push_back(json_string_value(stringJson.get()));
    }

    result = std::move(out);
    return Status();
}

Status
loginRecovery2(std::shared_ptr<Login> &result,
               LoginStore &store, DataSlice recovery2Key,
               const std::list<std::string> &answers,
               AuthError &authError)
{
    JsonPtr recovery2Auth;
    ABC_CHECK(recovery2AuthBuild(recovery2Auth, recovery2Key, answers));
    const auto recovery2Id = hmacSha256(store.username(), recovery2Key);

    // Grab the login information from the server:
    AuthJson authJson;
    LoginJson loginJson;
    ABC_CHECK(authJson.recovery2Set(store, recovery2Id, recovery2Auth));
    ABC_CHECK(loginServerLogin(loginJson, authJson, &authError));

    // Unlock recovery2Box:
    DataChunk dataKey;
    ABC_CHECK(loginJson.recovery2Box().decrypt(dataKey, recovery2Key));

    // Create the Login object:
    ABC_CHECK(Login::createOnline(result, store, dataKey, loginJson));
    return Status();
}

Status
loginRecovery2Set(DataChunk &result, Login &login,
                  const std::list<std::string> &questions,
                  const std::list<std::string> &answers)
{
    DataChunk recovery2Key;
    if (!loginRecovery2Key(recovery2Key, login.paths))
    {
        ABC_CHECK(randomData(recovery2Key, 32));
        ABC_CHECK(loginRecovery2KeySave(recovery2Key, login.paths));
    }

    // Create recovery2Auth:
    JsonPtr recovery2Auth;
    ABC_CHECK(recovery2AuthBuild(recovery2Auth, recovery2Key, answers));
    const auto recovery2Id = hmacSha256(login.store.username(), recovery2Key);

    // Create question2Box:
    JsonBox question2Box;
    JsonArray arrayJson;
    for (const auto &question: questions)
        ABC_CHECK(arrayJson.append(json_string(question.c_str())));
    ABC_CHECK(question2Box.encrypt(arrayJson.encode(), recovery2Key));

    // Create recovery2Box:
    JsonBox recovery2Box;
    ABC_CHECK(recovery2Box.encrypt(login.dataKey(), recovery2Key));

    // Create recovery2KeyBox:
    JsonBox recovery2KeyBox;
    ABC_CHECK(recovery2KeyBox.encrypt(recovery2Key, login.dataKey()));

    // Change the server login:
    AuthJson authJson;
    ABC_CHECK(authJson.loginSet(login));
    ABC_CHECK(loginServerRecovery2Set(authJson,
                                      recovery2Id, recovery2Auth,
                                      question2Box, recovery2Box,
                                      recovery2KeyBox));

    result = recovery2Key;
    return Status();
}

} // namespace abcd
