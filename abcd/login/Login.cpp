/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Login.hpp"
#include "LoginStore.hpp"
#include "json/AuthJson.hpp"
#include "json/KeyJson.hpp"
#include "json/LoginJson.hpp"
#include "json/LoginPackages.hpp"
#include "server/LoginServer.hpp"
#include "../crypto/Crypto.hpp"
#include "../crypto/Encoding.hpp"
#include "../crypto/Random.hpp"
#include "../json/JsonArray.hpp"
#include "../json/JsonBox.hpp"
#include "../util/FileIO.hpp"
#include "../util/Sync.hpp"
#include <bitcoin/bitcoin.hpp>
#include <ctype.h>

namespace abcd {

const std::string infoKeyHmacKey("infoKey");

Status
Login::createOffline(std::shared_ptr<Login> &result,
                     LoginStore &store, DataSlice dataKey)
{
    std::shared_ptr<Login> out(new Login(store, dataKey));
    ABC_CHECK(out->loadOffline());

    result = std::move(out);
    return Status();
}

Status
Login::createOnline(std::shared_ptr<Login> &result,
                    LoginStore &store, DataSlice dataKey, LoginReplyJson loginJson)
{
    std::shared_ptr<Login> out(new Login(store, dataKey));
    ABC_CHECK(out->loadOnline(loginJson));

    result = std::move(out);
    return Status();
}

Status
Login::createNew(std::shared_ptr<Login> &result,
                 LoginStore &store, const char *password)
{
    DataChunk dataKey;
    ABC_CHECK(randomData(dataKey, DATA_KEY_LENGTH));
    std::shared_ptr<Login> out(new Login(store, dataKey));
    ABC_CHECK(out->createNew(password));

    result = std::move(out);
    return Status();
}

Status
Login::update()
{
    AuthJson authJson;
    LoginReplyJson loginJson;
    ABC_CHECK(authJson.loginSet(*this));
    ABC_CHECK(loginServerLogin(loginJson, authJson));
    ABC_CHECK(loginJson.save(*this));

    return Status();
}

DataChunk
Login::passwordAuth() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return passwordAuth_;
}

Status
Login::passwordAuthSet(DataSlice passwordAuth)
{
    std::lock_guard<std::mutex> lock(mutex_);
    passwordAuth_ = DataChunk(passwordAuth.begin(), passwordAuth.end());
    return Status();
}

Status
Login::repoFind(JsonPtr &result, const std::string &type, bool create)
{
    // Search the on-disk array:
    LoginStashJson stashJson;
    if (stashJson.load(paths.stashPath()))
    {
        auto keyBoxesJson = stashJson.keyBoxes();
        size_t keyBoxesSize = keyBoxesJson.size();
        for (size_t i = 0; i < keyBoxesSize; i++)
        {
            JsonBox boxJson(keyBoxesJson[i]);

            DataChunk keyBytes;
            KeyJson keyJson;
            ABC_CHECK(boxJson.decrypt(keyBytes, dataKey_));
            ABC_CHECK(keyJson.decode(toString(keyBytes)));

            if (keyJson.typeOk() && type == keyJson.type())
            {
                result = keyJson.keys();
                return Status();
            }
        }
    }
    else if (repoTypeAirbitzAccount != type)
    {
        return ABC_ERROR(ABC_CC_FileDoesNotExist,
                         "Non-Airbitz app running on an offline Airbitz account");
    }

    // If this is an Airbitz account, try the legacy `syncKey`:
    if (repoTypeAirbitzAccount == type)
    {
        if (stashJson.syncKeyBox().ok())
        {
            DataChunk syncKey;
            ABC_CHECK(stashJson.syncKeyBox().decrypt(syncKey, dataKey_));

            AccountRepoJson repoJson;
            ABC_CHECK(repoJson.syncKeySet(base64Encode(syncKey)));
            ABC_CHECK(repoJson.dataKeySet(base64Encode(dataKey_)));

            result = repoJson;
            return Status();
        }
    }

    // If we are still here, nothing matched:
    if (create)
    {
        // Make the keys:
        DataChunk dataKey;
        DataChunk syncKey;
        ABC_CHECK(randomData(dataKey, DATA_KEY_LENGTH));
        ABC_CHECK(randomData(syncKey, SYNC_KEY_LENGTH));
        AccountRepoJson repoJson;
        ABC_CHECK(repoJson.syncKeySet(base64Encode(syncKey)));
        ABC_CHECK(repoJson.dataKeySet(base64Encode(dataKey_)));

        // Make the metadata:
        DataChunk id;
        ABC_CHECK(randomData(id, keyIdLength));
        KeyJson keyJson;
        ABC_CHECK(keyJson.idSet(base64Encode(id)));
        ABC_CHECK(keyJson.typeSet(type));
        ABC_CHECK(keyJson.keysSet(repoJson));

        // Encrypt the metadata:
        JsonBox keyBox;
        ABC_CHECK(keyBox.encrypt(keyJson.encode(), dataKey_));

        // Push the wallet to the server:
        AuthJson authJson;
        ABC_CHECK(authJson.loginSet(*this));
        ABC_CHECK(loginServerKeyAdd(authJson, keyBox, base16Encode(syncKey)));

        // Save to disk:
        if (!stashJson.keyBoxes().ok())
            ABC_CHECK(stashJson.keyBoxesSet(JsonArray()));
        ABC_CHECK(stashJson.keyBoxes().append(keyBox));
        ABC_CHECK(stashJson.save(paths.stashPath()));

        result = repoJson;
        return Status();
    }

    return ABC_ERROR(ABC_CC_AccountDoesNotExist, "No such repo");
}

Status
Login::makeEdgeLogin(JsonPtr &result, const std::string &appId,
                     const std::string &pin)
{
    result = JsonPtr();

    // Try 1: Use what we have:
    makeEdgeLoginLocal(result, appId).log(); // Failure is fine
    if (result) return Status();

    // Try 2: Sync with the server:
    ABC_CHECK(update());
    makeEdgeLoginLocal(result, appId).log(); // Failure is fine
    if (result) return Status();

    // Try 3: Make a new login:
    {
        DataChunk loginKey;
        ABC_CHECK(randomData(loginKey, DATA_KEY_LENGTH));
        JsonBox parentBox;
        ABC_CHECK(parentBox.encrypt(loginKey, dataKey_));

        // Make the access credentials:
        DataChunk loginId;
        DataChunk loginAuth;
        ABC_CHECK(randomData(loginId, scryptDefaultSize));
        ABC_CHECK(randomData(loginAuth, scryptDefaultSize));
        JsonBox loginAuthBox;
        ABC_CHECK(loginAuthBox.encrypt(loginAuth, loginKey));

        // Set up the outgoing Login object:
        LoginReplyJson server;
        ABC_CHECK(server.appIdSet(appId));
        ABC_CHECK(server.loginIdSet(base64Encode(loginId)));
        ABC_CHECK(server.loginAuthBoxSet(loginAuthBox));
        ABC_CHECK(server.parentBoxSet(parentBox));
        LoginStashJson stash = server.clone();
        ABC_CHECK(server.set("loginAuth", base64Encode(loginAuth)));

        // Set up the PIN, if we have one:
        if (pin.size())
        {
            DataChunk pin2Key;
            ABC_CHECK(randomData(pin2Key, 32));
            const auto pin2Id = hmacSha256(store.username(), pin2Key);
            const auto pin2Auth = hmacSha256(pin, pin2Key);

            // Create pin2Box:
            JsonBox pin2Box;
            ABC_CHECK(pin2Box.encrypt(loginKey, pin2Key));

            // Create pin2KeyBox:
            JsonBox pin2KeyBox;
            ABC_CHECK(pin2KeyBox.encrypt(pin2Key, loginKey));

            // Set up the server login:
            ABC_CHECK(server.set("pin2Id", base64Encode(pin2Id)));
            ABC_CHECK(server.set("pin2Auth", base64Encode(pin2Auth)));
            ABC_CHECK(server.pin2BoxSet(pin2Box));
            ABC_CHECK(server.pin2KeyBoxSet(pin2KeyBox));
            ABC_CHECK(stash.pin2KeySet(base64Encode(pin2Key)));
        }

        // Write to server:
        AuthJson authJson;
        ABC_CHECK(authJson.loginSet(*this));
        ABC_CHECK(loginServerCreateChildLogin(authJson, server));

        // Save to disk:
        LoginStashJson stashJson;
        ABC_CHECK(stashJson.load(paths.stashPath()));
        if (!stashJson.children().ok())
            ABC_CHECK(stashJson.childrenSet(JsonArray()));
        ABC_CHECK(stashJson.children().append(stash));
        ABC_CHECK(stashJson.save(paths.stashPath()));
    }

    ABC_CHECK(makeEdgeLoginLocal(result, appId).log());
    if (!result)
        return ABC_ERROR(ABC_CC_Error, "Empty edge login after creation.");
    return Status();
}

Status
Login::makeEdgeLoginLocal(JsonPtr &result, const std::string &appId)
{
    LoginStashJson stashJson, pruned;
    ABC_CHECK(stashJson.load(paths.stashPath()));
    ABC_CHECK(stashJson.makeEdgeLogin(pruned, appId));
    DataChunk loginKey;
    ABC_CHECK(stashJson.findLoginKey(loginKey, dataKey_, appId));

    JsonObject out;
    ABC_CHECK(out.set("appId", appId));
    ABC_CHECK(out.set("loginKey", base64Encode(loginKey)));
    ABC_CHECK(out.set("loginStash", pruned));

    result = out;
    return Status();
}

Login::Login(LoginStore &store, DataSlice dataKey):
    store(store),
    parent_(store.shared_from_this()),
    dataKey_(dataKey.begin(), dataKey.end())
{}

Status
Login::createNew(const char *password)
{
    LoginPackage loginPackage;
    JsonSnrp snrp;

    // Set up care package:
    CarePackage carePackage;
    ABC_CHECK(snrp.create());
    ABC_CHECK(carePackage.passwordKeySnrpSet(snrp));

    // Set up syncKey:
    DataChunk syncKey;
    JsonBox syncKeyBox;
    ABC_CHECK(randomData(syncKey, SYNC_KEY_LENGTH));
    ABC_CHECK(syncKeyBox.encrypt(syncKey, dataKey_));
    ABC_CHECK(loginPackage.syncKeyBoxSet(syncKeyBox));

    // Set up passwordAuth (LP1):
    if (password)
    {
        std::string LP = store.username() + password;

        // Generate passwordAuth:
        ABC_CHECK(usernameSnrp().hash(passwordAuth_, LP));

        // We have a password, so use it to encrypt dataKey:
        DataChunk passwordKey;
        JsonBox passwordBox;
        ABC_CHECK(carePackage.passwordKeySnrp().hash(passwordKey, LP));
        ABC_CHECK(passwordBox.encrypt(dataKey_, passwordKey));
        ABC_CHECK(loginPackage.passwordBoxSet(passwordBox));
    }
    else
    {
        // Generate passwordAuth:
        ABC_CHECK(randomData(passwordAuth_, scryptDefaultSize));
    }
    JsonBox passwordAuthBox;
    ABC_CHECK(passwordAuthBox.encrypt(passwordAuth_, dataKey_));
    ABC_CHECK(loginPackage.passwordAuthBoxSet(passwordAuthBox));

    // Create the account and repo on server:
    ABC_CHECK(loginServerCreate(store, passwordAuth_,
                                carePackage, loginPackage, base16Encode(syncKey)));

    // Set up the on-disk login:
    ABC_CHECK(store.paths(paths, true));
    ABC_CHECK(carePackage.save(paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(paths.loginPackagePath()));
    ABC_CHECK(rootKeyUpgrade());

    // Save the bare minimum needed to access the Airbitz account:
    LoginStashJson stashJson;
    ABC_CHECK(stashJson.loginIdSet(base64Encode(store.userId())));
    ABC_CHECK(stashJson.syncKeyBoxSet(syncKeyBox));
    stashJson.save(paths.stashPath());

    // Latch the account:
    ABC_CHECK(loginServerActivate(*this));

    return Status();
}

Status
Login::loadOffline()
{
    ABC_CHECK(store.paths(paths, true));

    LoginPackage loginPackage;
    ABC_CHECK(loginPackage.load(paths.loginPackagePath()));
    ABC_CHECK(loginPackage.passwordAuthBox().decrypt(passwordAuth_, dataKey_));

    // Look for an existing rootKeyBox:
    JsonBox rootKeyBox;
    if (fileExists(paths.rootKeyPath()))
    {
        ABC_CHECK(rootKeyBox.load(paths.rootKeyPath()));
    }
    else
    {
        // Try asking the server:
        AuthJson authJson;
        LoginReplyJson loginJson;
        ABC_CHECK(authJson.loginSet(*this));
        ABC_CHECK(loginServerLogin(loginJson, authJson));
        ABC_CHECK(loginJson.save(*this));
        rootKeyBox = loginJson.rootKeyBox();
    }
    // Otherwise, there just isn't one.

    // Extract the rootKey:
    if (rootKeyBox.ok())
        ABC_CHECK(rootKeyBox.decrypt(rootKey_, dataKey_));
    else
        ABC_CHECK(rootKeyUpgrade());

    return Status();
}

Status
Login::loadOnline(LoginReplyJson loginJson)
{
    ABC_CHECK(store.paths(paths, true));
    ABC_CHECK(loginJson.save(*this));

    ABC_CHECK(loginJson.passwordAuthBox().decrypt(passwordAuth_, dataKey_));

    // Extract the rootKey:
    auto rootKeyBox = loginJson.rootKeyBox();
    if (!rootKeyBox.ok() && fileExists(paths.rootKeyPath()))
        return ABC_ERROR(ABC_CC_Error,
                         "The account has a rootKey, but it's not on the server.");
    if (rootKeyBox.ok())
        ABC_CHECK(rootKeyBox.decrypt(rootKey_, dataKey_));
    else
        ABC_CHECK(rootKeyUpgrade());

    return Status();
}

Status
Login::rootKeyUpgrade()
{
    // Create a BIP39 mnemonic, and use it to derive the rootKey:
    DataChunk entropy;
    ABC_CHECK(randomData(entropy, 256/8));
    auto mnemonic = bc::create_mnemonic(entropy, bc::language::en);
    auto rootKeyRaw = bc::decode_mnemonic(mnemonic);
    rootKey_ = DataChunk(rootKeyRaw.begin(), rootKeyRaw.end());

    // Pack the keys into various boxes:
    JsonBox rootKeyBox;
    ABC_CHECK(rootKeyBox.encrypt(rootKey_, dataKey_));
    JsonBox mnemonicBox, dataKeyBox;
    auto infoKey = bc::hmac_sha256_hash(rootKey_, DataSlice(infoKeyHmacKey));
    ABC_CHECK(mnemonicBox.encrypt(bc::join(mnemonic), infoKey));
    ABC_CHECK(dataKeyBox.encrypt(dataKey_, infoKey));

    // Upgrade the account on the server:
    ABC_CHECK(loginServerAccountUpgrade(*this,
                                        rootKeyBox, mnemonicBox, dataKeyBox));
    ABC_CHECK(rootKeyBox.save(paths.rootKeyPath()));

    return Status();
}

} // namespace abcd
