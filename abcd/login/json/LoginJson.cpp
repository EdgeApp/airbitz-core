/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginJson.hpp"
#include "LoginPackages.hpp"
#include "../Login.hpp"
#include "../LoginStore.hpp"
#include "../LoginPin2.hpp"
#include "../LoginRecovery2.hpp"
#include "../../crypto/Encoding.hpp"

namespace abcd {

Status
LoginReplyJson::save(const Login &login)
{
    CarePackage carePackage;
    LoginPackage loginPackage;

    // Password:
    if (passwordAuthBox().ok())
        ABC_CHECK(loginPackage.passwordAuthBoxSet(passwordAuthBox()));
    if (passwordBox().ok())
        ABC_CHECK(loginPackage.passwordBoxSet(passwordBox()));
    if (passwordKeySnrp().ok())
        ABC_CHECK(carePackage.passwordKeySnrpSet(passwordKeySnrp()));

    // Recovery v1:
    if (questionBox().ok())
        ABC_CHECK(carePackage.questionBoxSet(questionBox()));
    if (questionKeySnrp().ok())
        ABC_CHECK(carePackage.questionKeySnrpSet(questionKeySnrp()));
    if (recoveryBox().ok())
        ABC_CHECK(loginPackage.recoveryBoxSet(recoveryBox()));
    if (recoveryKeySnrp().ok())
        ABC_CHECK(carePackage.recoveryKeySnrpSet(recoveryKeySnrp()));

    // Keys:
    if (rootKeyBox().ok())
        ABC_CHECK(rootKeyBox().save(login.paths.rootKeyPath()));
    if (syncKeyBox().ok())
        ABC_CHECK(loginPackage.syncKeyBoxSet(syncKeyBox()));

    // Keys to save unencrypted:
    DataChunk pin2Key;
    if (pin2KeyBox().decrypt(pin2Key, login.dataKey()))
        ABC_CHECK(loginPin2KeySave(pin2Key, login.paths));

    DataChunk recovery2Key;
    if (recovery2KeyBox().decrypt(recovery2Key, login.dataKey()))
        ABC_CHECK(loginRecovery2KeySave(recovery2Key, login.paths));

    // Store:
    LoginStashJson stashJson;
    ABC_CHECK(makeLoginStashJson(stashJson, login.dataKey(),
                                 login.store.username()));
    ABC_CHECK(stashJson.save(login.paths.stashPath()));

    // Write to disk:
    ABC_CHECK(carePackage.save(login.paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(login.paths.loginPackagePath()));

    return Status();
}

Status
LoginReplyJson::makeLoginStashJson(LoginStashJson &result, DataSlice dataKey,
                                   const std::string &username)
{
    LoginStashJson out;

    // Copy everything we can:
    const std::vector<std::string> keys =
    {
        "appId",
        "loginId",
        "loginAuthBox",
        "parentBox",
        "passwordAuthBox",
        "passwordBox",
        "passwordKeySnrp",
        "questionBox",
        "questionKeySnrp",
        "recoveryBox",
        "recoveryKeySnrp",
        "mnemonicBox",
        "rootKeyBox",
        "syncKeyBox",
        "keyBoxes"
    };
    ABC_CHECK(out.pick(*this, keys));

    if (username.size())
        ABC_CHECK(out.usernameSet(username));

    // Decrypt keys:
    DataChunk pin2Key;
    if (pin2KeyBox().decrypt(pin2Key, dataKey))
        ABC_CHECK(out.pin2KeySet(base58Encode(pin2Key)));

    DataChunk recovery2Key;
    if (recovery2KeyBox().decrypt(recovery2Key, dataKey))
        ABC_CHECK(out.recovery2KeySet(base58Encode(recovery2Key)));

    // Recurse into children:
    auto childrenJson = children();
    JsonArray storeChildrenJson;
    size_t childrenSize = childrenJson.size();
    for (size_t i = 0; i < childrenSize; i++)
    {
        LoginReplyJson childJson(childrenJson[i]);
        DataChunk childDataKey;
        ABC_CHECK(childJson.parentBox().decrypt(childDataKey, dataKey));

        LoginStashJson storeChildJson;
        ABC_CHECK(childJson.makeLoginStashJson(storeChildJson, childDataKey,
                                               ""));
        ABC_CHECK(storeChildrenJson.append(storeChildJson));
    }
    ABC_CHECK(out.childrenSet(storeChildrenJson));

    result = std::move(out);
    return Status();
}

Status
LoginStashJson::makeEdgeLogin(LoginStashJson &result, const std::string &appId)
{
    // If this is the login we are looking for, just return as-is:
    if (this->appId() == appId)
    {
        result = *this;
        return Status();
    }

    // Do any children apply to the appId?
    LoginStashJson relevantChild = JsonPtr();
    auto childrenJson = children();
    size_t childrenSize = childrenJson.size();
    for (size_t i = 0; i < childrenSize; i++)
    {
        LoginStashJson childJson(childrenJson[i]);

        ABC_CHECK(childJson.makeEdgeLogin(relevantChild, appId));
        if (relevantChild)
            break;
    }

    // If we don't have relevant children, then we are irrelevant:
    if (!relevantChild)
    {
        result = JsonPtr();
        return Status();
    }

    // Trim down this node:
    LoginStashJson out;
    ABC_CHECK(out.pick(*this, {"username", "appId", "loginId"}));
    ABC_CHECK(out.childrenSet(JsonArray()));
    ABC_CHECK(out.children().append(relevantChild));

    result = out;
    return Status();
}

Status
LoginStashJson::findLoginKey(DataChunk &result, DataSlice dataKey,
                             const std::string &appId)
{
    // If this is the login we are looking for, attach the dataKey:
    if (this->appId() == appId)
    {
        result = DataChunk(dataKey.begin(), dataKey.end());
        return Status();
    }

    // Do any children apply to the appId?
    LoginStashJson relevantChild = JsonPtr();
    auto childrenJson = children();
    size_t childrenSize = childrenJson.size();
    for (size_t i = 0; i < childrenSize; i++)
    {
        LoginStashJson childJson(childrenJson[i]);
        DataChunk childDataKey;
        ABC_CHECK(childJson.parentBox().decrypt(childDataKey, dataKey));
        if (childJson.findLoginKey(result, childDataKey, appId))
            return Status();
    }

    return ABC_ERROR(ABC_CC_AccountDoesNotExist, "Cannot find appId");
}

} // namespace abcd
