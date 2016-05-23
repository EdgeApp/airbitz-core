/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Login.hpp"
#include "Lobby.hpp"
#include "LoginPackages.hpp"
#include "../auth/LoginServer.hpp"
#include "../crypto/Encoding.hpp"
#include "../crypto/Random.hpp"
#include "../json/JsonBox.hpp"
#include "../util/FileIO.hpp"
#include "../util/Sync.hpp"
#include <bitcoin/bitcoin.hpp>
#include <ctype.h>

namespace abcd {

const std::string infoKeyHmacKey("infoKey");

Status
Login::create(std::shared_ptr<Login> &result, Lobby &lobby, DataSlice dataKey,
              const LoginPackage &loginPackage, JsonBox rootKeyBox, bool offline)
{
    std::shared_ptr<Login> out(new Login(lobby, dataKey));
    ABC_CHECK(out->loadKeys(loginPackage, rootKeyBox, offline));

    result = std::move(out);
    return Status();
}

Status
Login::createNew(std::shared_ptr<Login> &result, Lobby &lobby,
                 const char *password)
{
    DataChunk dataKey;
    ABC_CHECK(randomData(dataKey, DATA_KEY_LENGTH));
    std::shared_ptr<Login> out(new Login(lobby, dataKey));
    ABC_CHECK(out->createNew(password));

    result = std::move(out);
    return Status();
}

std::string
Login::syncKey() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return base16Encode(syncKey_);
}

DataChunk
Login::authKey() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return authKey_;
}

Status
Login::authKeySet(DataSlice authKey)
{
    std::lock_guard<std::mutex> lock(mutex_);
    authKey_ = DataChunk(authKey.begin(), authKey.end());
    return Status();
}

Login::Login(Lobby &lobby, DataSlice dataKey):
    lobby(lobby),
    parent_(lobby.shared_from_this()),
    dataKey_(dataKey.begin(), dataKey.end())
{}

Status
Login::createNew(const char *password)
{
    LoginPackage loginPackage;
    JsonBox box;
    JsonSnrp snrp;

    // Set up care package:
    CarePackage carePackage;
    ABC_CHECK(snrp.create());
    ABC_CHECK(carePackage.snrp2Set(snrp));

    // Set up syncKey:
    ABC_CHECK(randomData(syncKey_, SYNC_KEY_LENGTH));
    ABC_CHECK(box.encrypt(syncKey_, dataKey_));
    ABC_CHECK(loginPackage.syncKeyBoxSet(box));

    // Set up authKey (LP1):
    if (password)
    {
        std::string LP = lobby.username() + password;

        // Generate authKey:
        ABC_CHECK(usernameSnrp().hash(authKey_, LP));

        // We have a password, so use it to encrypt dataKey:
        DataChunk passwordKey;
        ABC_CHECK(carePackage.snrp2().hash(passwordKey, LP));
        ABC_CHECK(box.encrypt(dataKey_, passwordKey));
        ABC_CHECK(loginPackage.passwordBoxSet(box));
    }
    else
    {
        // Generate authKey:
        ABC_CHECK(randomData(authKey_, scryptDefaultSize));
    }
    ABC_CHECK(box.encrypt(authKey_, dataKey_));
    ABC_CHECK(loginPackage.authKeyBoxSet(box));

    // Create the account and repo on server:
    ABC_CHECK(loginServerCreate(lobby, authKey_,
                                carePackage, loginPackage, base16Encode(syncKey_)));

    // Set up the on-disk login:
    ABC_CHECK(lobby.paths(paths, true));
    ABC_CHECK(carePackage.save(paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(paths.loginPackagePath()));
    ABC_CHECK(rootKeyUpgrade());

    // Latch the account:
    ABC_CHECK(loginServerActivate(*this));

    return Status();
}

Status
Login::loadKeys(const LoginPackage &loginPackage, JsonBox rootKeyBox,
                bool diskBased)
{
    ABC_CHECK(loginPackage.syncKeyBox().decrypt(syncKey_, dataKey_));
    ABC_CHECK(loginPackage.authKeyBox().decrypt(authKey_, dataKey_));

    ABC_CHECK(lobby.paths(paths, true));

    // Look for an existing rootKeyBox:
    if (!rootKeyBox)
    {
        if (fileExists(paths.rootKeyPath()))
        {
            if (diskBased)
                ABC_CHECK(rootKeyBox.load(paths.rootKeyPath()));
            else
                return ABC_ERROR(ABC_CC_Error,
                                 "The account has a rootKey, but it's not on the server.");
        }
        else if (diskBased)
        {
            // The server hasn't been asked yet, so do that now:
            LoginPackage unused;
            AuthError authError;
            ABC_CHECK(loginServerGetLoginPackage(lobby, authKey_, DataChunk(),
                                                 unused, rootKeyBox,
                                                 authError));

            // If the server had one, save it for the future:
            if (rootKeyBox)
                ABC_CHECK(rootKeyBox.save(paths.rootKeyPath()));
        }
        // Otherwise, there just isn't one.
    }

    // Extract the rootKey:
    if (rootKeyBox)
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
