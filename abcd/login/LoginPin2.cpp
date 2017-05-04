/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginPin2.hpp"
#include "Login.hpp"
#include "LoginStore.hpp"
#include "json/AuthJson.hpp"
#include "json/LoginJson.hpp"
#include "server/LoginServer.hpp"
#include "../crypto/Crypto.hpp"
#include "../crypto/Encoding.hpp"
#include "../crypto/Random.hpp"
#include "../json/JsonBox.hpp"
#include "../util/FileIO.hpp"

namespace abcd {

struct Pin2KeyJson:
    public JsonObject
{
    ABC_JSON_STRING(pin2Key, "pin2Key", "!bad")
};

static Status
loginPin2SetStash(LoginStashJson &stashJson, DataSlice loginKey,
                  const std::string &username, const std::string &pin,
                  Login *login = nullptr)
{
    // Only change the PIN if it is enabled for this login:
    if (stashJson.pin2KeyOk())
    {
        DataChunk pin2Key;
        ABC_CHECK(base64Decode(pin2Key, stashJson.pin2Key()));

        // Create pin2Auth:
        const auto pin2Id = hmacSha256(username, pin2Key);
        const auto pin2Auth = hmacSha256(pin, pin2Key);

        // Create pin2Box:
        JsonBox pin2Box;
        ABC_CHECK(pin2Box.encrypt(loginKey, pin2Key));

        // Create pin2KeyBox:
        JsonBox pin2KeyBox;
        ABC_CHECK(pin2KeyBox.encrypt(pin2Key, loginKey));

        // Change the server login:
        AuthJson authJson;
        if (login)
            ABC_CHECK(authJson.loginSet(*login));
        else
            ABC_CHECK(authJson.stashSet(stashJson, loginKey));
        ABC_CHECK(loginServerPin2Set(authJson,
                                     pin2Id, pin2Auth,
                                     pin2Box, pin2KeyBox));
    }

    // Recurse into children:
    auto childrenJson = stashJson.children();
    size_t childrenSize = childrenJson.size();
    for (size_t i = 0; i < childrenSize; i++)
    {
        LoginStashJson childJson(childrenJson[i]);
        DataChunk childLoginKey;
        ABC_CHECK(childJson.parentBox().decrypt(childLoginKey, loginKey));

        ABC_CHECK(loginPin2SetStash(childJson, childLoginKey, username, pin));
    }

    return Status();
}

Status
loginPin2Key(DataChunk &result, const AccountPaths &paths)
{
    Pin2KeyJson json;
    ABC_CHECK(json.load(paths.pin2KeyPath()));
    ABC_CHECK(base58Decode(result, json.pin2Key()));

    return Status();
}

Status
loginPin2KeySave(DataSlice pin2Key, const AccountPaths &paths)
{
    Pin2KeyJson json;
    ABC_CHECK(json.pin2KeySet(base58Encode(pin2Key)));
    ABC_CHECK(json.save(paths.pin2KeyPath()));

    return Status();
}

Status
loginPin2(std::shared_ptr<Login> &result,
          LoginStore &store, DataSlice pin2Key,
          const std::string &pin,
          AuthError &authError)
{
    const auto pin2Id = hmacSha256(store.username(), pin2Key);
    const auto pin2Auth = hmacSha256(pin, pin2Key);

    // Grab the login information from the server:
    AuthJson authJson;
    LoginReplyJson loginJson;
    ABC_CHECK(authJson.pin2Set(store, pin2Id, pin2Auth));
    ABC_CHECK(loginServerLogin(loginJson, authJson, &authError));

    // Unlock pin2Box:
    DataChunk dataKey;
    ABC_CHECK(loginJson.pin2Box().decrypt(dataKey, pin2Key));

    // Create the Login object:
    ABC_CHECK(Login::createOnline(result, store, dataKey, loginJson));
    return Status();
}

Status
loginPin2Set(DataChunk &result, Login &login,
             const std::string &pin)
{
    // Grab up-to-date keys from the server:
    ABC_CHECK(login.update());
    LoginStashJson stashJson;
    ABC_CHECK(stashJson.load(login.paths.stashPath()));

    // Make a key if there isn't one already:
    DataChunk pin2Key;
    if (!loginPin2Key(pin2Key, login.paths))
    {
        ABC_CHECK(randomData(pin2Key, 32));
        ABC_CHECK(stashJson.pin2KeySet(base64Encode(pin2Key)));
        ABC_CHECK(stashJson.save(login.paths.stashPath()));
        ABC_CHECK(loginPin2KeySave(pin2Key, login.paths));
    }

    // Change the PIN:
    ABC_CHECK(loginPin2SetStash(stashJson, login.dataKey(),
                                login.store.username(), pin,
                                &login));

    result = pin2Key;
    return Status();
}

Status
loginPin2Delete(Login &login)
{
    // Change the server login:
    AuthJson authJson;
    ABC_CHECK(authJson.loginSet(login));
    ABC_CHECK(loginServerPin2Delete(authJson));

    // Delete the saved key:
    fileDelete(login.paths.pin2KeyPath());

    return Status();
}

} // namespace abcd
