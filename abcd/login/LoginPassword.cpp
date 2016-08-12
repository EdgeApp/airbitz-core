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
#include "server/LoginServer.hpp"
#include "../Context.hpp"
#include "../json/JsonBox.hpp"

namespace abcd {

static Status
loginPasswordDisk(std::shared_ptr<Login> &result,
                  Lobby &lobby, const std::string &password)
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
    ABC_CHECK(carePackage.passwordKeySnrp().hash(passwordKey, LP));

    // Decrypt dataKey (unlocks the account):
    DataChunk dataKey;
    ABC_CHECK(loginPackage.passwordBox().decrypt(dataKey, passwordKey));

    // Create the Login object:
    std::shared_ptr<Login> out;
    ABC_CHECK(Login::create(out, lobby, dataKey,
                            loginPackage, JsonPtr(), true));

    result = std::move(out);
    return Status();
}

static Status
loginPasswordServer(std::shared_ptr<Login> &result,
                    Lobby &lobby, const std::string &password,
                    AuthError &authError)
{
    std::string LP = lobby.username() + password;

    // Get the CarePackage:
    CarePackage carePackage;
    ABC_CHECK(loginServerGetCarePackage(lobby, carePackage));

    // Make the passwordAuth (unlocks the server):
    DataChunk passwordAuth;
    ABC_CHECK(usernameSnrp().hash(passwordAuth, LP));

    // Get the LoginPackage:
    LoginPackage loginPackage;
    JsonPtr rootKeyBox;
    ABC_CHECK(loginServerGetLoginPackage(lobby, passwordAuth, DataSlice(),
                                         loginPackage, rootKeyBox,
                                         authError));

    // Make passwordKey (unlocks dataKey):
    DataChunk passwordKey;
    ABC_CHECK(carePackage.passwordKeySnrp().hash(passwordKey, LP));

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
              Lobby &lobby, const std::string &password,
              AuthError &authError)
{
    // Try the login both ways:
    if (!loginPasswordDisk(result, lobby, password))
        ABC_CHECK(loginPasswordServer(result, lobby, password, authError));

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
    DataChunk passwordAuth = login.passwordAuth();
    DataChunk oldLRA1;
    if (loginPackage.ELRA1())
    {
        ABC_CHECK(loginPackage.ELRA1().decrypt(oldLRA1, login.dataKey()));
    }

    // Update passwordKeySnrp:
    JsonSnrp snrp;
    ABC_CHECK(snrp.create());
    ABC_CHECK(carePackage.passwordKeySnrpSet(snrp));

    // Update EMK_LP2:
    DataChunk passwordKey;      // Unlocks dataKey
    ABC_CHECK(carePackage.passwordKeySnrp().hash(passwordKey, LP));
    JsonBox passwordBox;
    ABC_CHECK(passwordBox.encrypt(login.dataKey(), passwordKey));
    ABC_CHECK(loginPackage.passwordBoxSet(passwordBox));

    // Update ELP1:
    DataChunk newPasswordAuth;       // Unlocks the server
    ABC_CHECK(usernameSnrp().hash(newPasswordAuth, LP));
    JsonBox passwordAuthBox;
    ABC_CHECK(passwordAuthBox.encrypt(newPasswordAuth, login.dataKey()));
    ABC_CHECK(loginPackage.passwordAuthBoxSet(passwordAuthBox));

    // Change the server login:
    ABC_CHECK(loginServerChangePassword(login, newPasswordAuth, oldLRA1,
                                        carePackage, loginPackage));

    // Change the on-disk login:
    ABC_CHECK(login.passwordAuthSet(newPasswordAuth));
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
    ABC_CHECK(carePackage.passwordKeySnrp().hash(passwordKey, LP));

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
