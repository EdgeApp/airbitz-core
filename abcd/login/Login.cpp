/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Login.hpp"
#include "LoginPackages.hpp"
#include "LoginStore.hpp"
#include "server/AuthJson.hpp"
#include "server/LoginJson.hpp"
#include "server/LoginServer.hpp"
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
                    LoginStore &store, DataSlice dataKey, LoginJson loginJson)
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
Login::repoFind(RepoInfo &result, const std::string &type, bool create)
{
    // Search the on-disk array:
    JsonArray reposJson;
    if (reposJson.load(paths.reposPath()))
    {
        size_t reposSize = reposJson.size();
        for (size_t i = 0; i < reposSize; i++)
        {
            RepoJson repoJson(reposJson[i]);
            ABC_CHECK(repoJson.typeOk());
            if (type == repoJson.type())
            {
                ABC_CHECK(repoJson.decode(result, dataKey_));
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
        LoginPackage loginPackage;
        ABC_CHECK(loginPackage.load(paths.loginPackagePath()));
        if (loginPackage.syncKeyBox().ok())
        {
            DataChunk syncKey;
            ABC_CHECK(loginPackage.syncKeyBox().decrypt(syncKey, dataKey_));

            result = RepoInfo
            {
                type, dataKey_, base16Encode(syncKey)
            };
            return Status();
        }
    }

    // If we are still here, nothing matched:
    if (create)
    {
        // Make the keys:
        RepoInfo repoInfo;
        DataChunk syncKey;
        repoInfo.type = type;
        ABC_CHECK(randomData(repoInfo.dataKey, DATA_KEY_LENGTH));
        ABC_CHECK(randomData(syncKey, SYNC_KEY_LENGTH));
        repoInfo.syncKey = base16Encode(syncKey);

        // Push the wallet to the server:
        AuthJson authJson;
        RepoJson repoJson;
        ABC_CHECK(authJson.loginSet(*this));
        ABC_CHECK(repoJson.encode(repoInfo, dataKey_));
        ABC_CHECK(loginServerWalletCreate(*this, repoInfo.syncKey));
        ABC_CHECK(loginServerReposAdd(authJson, repoJson));
        ABC_CHECK(loginServerWalletActivate(*this, repoInfo.syncKey));

        // Save to disk:
        ABC_CHECK(reposJson.append(repoJson));
        ABC_CHECK(reposJson.save(paths.reposPath()));

        result = repoInfo;
        return Status();
    }

    return ABC_ERROR(ABC_CC_AccountDoesNotExist, "No such repo");
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
    JsonArray reposJson;
    ABC_CHECK(reposJson.save(paths.reposPath()));

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
        LoginJson loginJson;
        ABC_CHECK(authJson.loginSet(*this));
        ABC_CHECK(loginServerLogin(loginJson, authJson));
        ABC_CHECK(loginJson.save(paths, dataKey_));
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
Login::loadOnline(LoginJson loginJson)
{
    ABC_CHECK(store.paths(paths, true));
    ABC_CHECK(loginJson.save(paths, dataKey_));

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
