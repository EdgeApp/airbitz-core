/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginPassword.hpp"
#include "Lobby.hpp"
#include "Login.hpp"
#include "LoginPackages.hpp"
#include "../Context.hpp"
#include "../auth/LoginServer.hpp"
#include "../json/JsonBox.hpp"

namespace abcd {

static Status
loginPasswordDisk(std::shared_ptr<Login> &result,
                  Lobby &lobby,
                  const std::string &password)
{
    std::string LP = lobby.username() + password;

    AccountPaths paths;
    ABC_CHECK(lobby.paths(paths));

    // Load the packages:
    CarePackage carePackage;
    LoginPackage loginPackage;
    ABC_CHECK(carePackage.load(paths.carePackagePath()));
    ABC_CHECK(loginPackage.load(paths.loginPackagePath()));

    // Make passwordKey (unlocks dataKey):
    DataChunk passwordKey;
    ABC_CHECK(carePackage.snrp2().hash(passwordKey, LP));

    // Decrypt dataKey (unlocks the account):
    DataChunk dataKey;
    ABC_CHECK(loginPackage.passwordBox().decrypt(dataKey, passwordKey));

    // Create the Login object:
    std::shared_ptr<Login> out;
    ABC_CHECK(Login::create(out, lobby, dataKey,
                            loginPackage, JsonBox(), true));

    result = std::move(out);
    return Status();
}

static Status
loginPasswordServer(std::shared_ptr<Login> &result,
                    Lobby &lobby,
                    const std::string &password)
{
    std::string LP = lobby.username() + password;

    // Get the CarePackage:
    CarePackage carePackage;
    ABC_CHECK(loginServerGetCarePackage(lobby, carePackage));

    // Make the authKey (unlocks the server):
    DataChunk authKey;
    ABC_CHECK(usernameSnrp().hash(authKey, LP));

    // Get the LoginPackage:
    LoginPackage loginPackage;
    JsonPtr rootKeyBox;
    ABC_CHECK(loginServerGetLoginPackage(lobby, authKey, U08Buf(),
                                         loginPackage, rootKeyBox));

    // Make passwordKey (unlocks dataKey):
    DataChunk passwordKey;
    ABC_CHECK(carePackage.snrp2().hash(passwordKey, LP));

    // Decrypt dataKey (unlocks the account):
    DataChunk dataKey;
    ABC_CHECK(loginPackage.passwordBox().decrypt(dataKey, passwordKey));

    // Create the Login object:
    std::shared_ptr<Login> out;
    ABC_CHECK(Login::create(out, lobby, dataKey,
                            loginPackage, rootKeyBox, false));

    // Set up the on-disk login:
    ABC_CHECK(carePackage.save(out->paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(out->paths.loginPackagePath()));

    result = std::move(out);
    return Status();
}

Status
loginPassword(std::shared_ptr<Login> &result,
              Lobby &lobby,
              const std::string &password)
{
    // Try the login both ways:
    if (!loginPasswordDisk(result, lobby, password))
        ABC_CHECK(loginPasswordServer(result, lobby, password));

    return Status();
}

Status
loginPasswordSet(Login &login, const std::string &password)
{
    std::string LP = login.lobby.username() + password;

    // Load the packages:
    CarePackage carePackage;
    LoginPackage loginPackage;
    ABC_CHECK(carePackage.load(login.paths.carePackagePath()));
    ABC_CHECK(loginPackage.load(login.paths.loginPackagePath()));

    // Load the old keys:
    DataChunk authKey = login.authKey();
    DataChunk oldLRA1;
    if (loginPackage.ELRA1())
    {
        ABC_CHECK(loginPackage.ELRA1().decrypt(oldLRA1, login.dataKey()));
    }

    // Update SNRP2:
    JsonSnrp snrp;
    ABC_CHECK(snrp.create());
    ABC_CHECK(carePackage.snrp2Set(snrp));

    // Update EMK_LP2:
    DataChunk passwordKey;      // Unlocks dataKey
    ABC_CHECK(carePackage.snrp2().hash(passwordKey, LP));
    JsonBox box;
    ABC_CHECK(box.encrypt(login.dataKey(), passwordKey));
    ABC_CHECK(loginPackage.passwordBoxSet(box));

    // Update ELP1:
    DataChunk newAuthKey;       // Unlocks the server
    ABC_CHECK(usernameSnrp().hash(newAuthKey, LP));
    ABC_CHECK(box.encrypt(newAuthKey, login.dataKey()));
    ABC_CHECK(loginPackage.authKeyBoxSet(box));

    // Change the server login:
    ABC_CHECK(loginServerChangePassword(login, newAuthKey, oldLRA1,
                                        carePackage, loginPackage));

    // Change the on-disk login:
    ABC_CHECK(login.authKeySet(newAuthKey));
    ABC_CHECK(carePackage.save(login.paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(login.paths.loginPackagePath()));

    return Status();
}

Status
loginPasswordOk(bool &result, Login &login, const std::string &password)
{
    std::string LP = login.lobby.username() + password;

    // Load the packages:
    CarePackage carePackage;
    LoginPackage loginPackage;
    ABC_CHECK(carePackage.load(login.paths.carePackagePath()));
    ABC_CHECK(loginPackage.load(login.paths.loginPackagePath()));

    // Make passwordKey (unlocks dataKey):
    DataChunk passwordKey;
    ABC_CHECK(carePackage.snrp2().hash(passwordKey, LP));

    // Try to decrypt dataKey (unlocks the account):
    DataChunk dataKey;
    result = !!loginPackage.passwordBox().decrypt(dataKey, passwordKey);

    return Status();
}

Status
loginPasswordExists(bool &result, const std::string &username)
{
    std::string fixed;
    ABC_CHECK(Lobby::fixUsername(fixed, username));
    AccountPaths paths;
    ABC_CHECK(gContext->paths.accountDir(paths, fixed));

    LoginPackage loginPackage;
    ABC_CHECK(loginPackage.load(paths.loginPackagePath()));

    result = !!loginPackage.passwordBox().get();
    return Status();
}

} // namespace abcd
