/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginPin.hpp"
#include "Login.hpp"
#include "LoginStore.hpp"
#include "json/LoginPackages.hpp"
#include "server/LoginServer.hpp"
#include "../Context.hpp"
#include "../crypto/Encoding.hpp"
#include "../crypto/Random.hpp"
#include "../json/JsonBox.hpp"
#include "../json/JsonObject.hpp"
#include "../util/FileIO.hpp"

namespace abcd {

#define KEY_LENGTH 32

/**
 * A round-trippable representation of the PIN-based re-login file.
 */
struct PinLocal:
    public JsonObject
{
    ABC_JSON_VALUE(pinBox,      "EMK_PINK", JsonBox)
    ABC_JSON_STRING(pinAuthId,  "DID",      nullptr)
    ABC_JSON_INTEGER(expires,   "Expires",  0)

    Status
    pinAuthIdDecode(DataChunk &result)
    {
        ABC_CHECK(pinAuthIdOk());
        ABC_CHECK(base64Decode(result, pinAuthId()));
        return Status();
    }
};

Status
loginPinExists(bool &result, const std::string &username)
{
    std::string fixed;
    ABC_CHECK(LoginStore::fixUsername(fixed, username));
    AccountPaths paths;
    ABC_CHECK(gContext->paths.accountDir(paths, fixed));

    PinLocal local;
    result = !!local.load(paths.pinPackagePath());
    return Status();
}

Status
loginPinDelete(LoginStore &store)
{
    AccountPaths paths;
    if (store.paths(paths))
        ABC_CHECK(fileDelete(paths.pinPackagePath()));

    return Status();
}

Status
loginPin(std::shared_ptr<Login> &result,
         LoginStore &store, const std::string &pin,
         AuthError &authError)
{
    std::string LPIN = store.username() + pin;

    AccountPaths paths;
    ABC_CHECK(store.paths(paths));

    // Load the packages:
    CarePackage carePackage;
    PinLocal local;
    ABC_CHECK(carePackage.load(paths.carePackagePath()));
    ABC_CHECK(local.load(paths.pinPackagePath()));
    DataChunk pinAuthId;
    ABC_CHECK(local.pinAuthIdDecode(pinAuthId));

    // Get EPINK from the server:
    std::string EPINK;
    DataChunk pinAuthKey;       // Unlocks the server
    JsonBox pinKeyBox;          // Holds pinKey
    ABC_CHECK(usernameSnrp().hash(pinAuthKey, LPIN));
    ABC_CHECK(loginServerGetPinPackage(pinAuthId, pinAuthKey, EPINK,
                                       authError));
    ABC_CHECK(pinKeyBox.decode(EPINK));

    // Decrypt dataKey:
    DataChunk pinKeyKey;        // Unlocks pinKey
    DataChunk pinKey;           // Unlocks dataKey
    DataChunk dataKey;          // Unlocks the account
    ABC_CHECK(carePackage.passwordKeySnrp().hash(pinKeyKey, LPIN));
    ABC_CHECK(pinKeyBox.decrypt(pinKey, pinKeyKey));
    ABC_CHECK(local.pinBox().decrypt(dataKey, pinKey));

    // Create the Login object:
    ABC_CHECK(Login::createOffline(result, store, dataKey));
    return Status();
}

} // namespace abcd
