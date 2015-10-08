/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Login.hpp"
#include "Lobby.hpp"
#include "LoginDir.hpp"
#include "../auth/LoginServer.hpp"
#include "../crypto/Encoding.hpp"
#include "../crypto/Random.hpp"
#include "../json/JsonBox.hpp"
#include "../util/FileIO.hpp"
#include "../util/Sync.hpp"
#include "../util/Util.hpp"
#include <ctype.h>

namespace abcd {

Status
Login::create(std::shared_ptr<Login> &result, Lobby &lobby, DataSlice dataKey,
    const LoginPackage &loginPackage)
{
    std::shared_ptr<Login> out(new Login(lobby, dataKey));
    ABC_CHECK(out->loadKeys(loginPackage));

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
    ABC_CHECK(lobby.dirCreate());
    ABC_CHECK(carePackage.save(lobby.carePackageName()));
    ABC_CHECK(loginPackage.save(lobby.loginPackageName()));

    // Latch the account:
    ABC_CHECK(loginServerActivate(*this));

    return Status();
}

Status
Login::loadKeys(const LoginPackage &loginPackage)
{
    ABC_CHECK(loginPackage.syncKeyBox().decrypt(syncKey_, dataKey_));
    ABC_CHECK(loginPackage.authKeyBox().decrypt(authKey_, dataKey_));

    ABC_CHECK(lobby.dirCreate());
    if (fileExists(lobby.dir() + "tempRootKey.json"))
    {
        JsonBox box;
        ABC_CHECK(box.load(lobby.dir() + "tempRootKey.json"));
        ABC_CHECK(box.decrypt(rootKey_, dataKey_));
    }
    else
    {
        JsonBox box;
        ABC_CHECK(randomData(rootKey_, 256));
        ABC_CHECK(box.encrypt(rootKey_, dataKey_));
        ABC_CHECK(box.save(lobby.dir() + "tempRootKey.json"));
    }

    return Status();
}

} // namespace abcd
