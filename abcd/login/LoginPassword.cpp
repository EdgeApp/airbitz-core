/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginPassword.hpp"
#include "Login.hpp"
#include "LoginStore.hpp"
#include "json/AuthJson.hpp"
#include "json/LoginJson.hpp"
#include "json/LoginPackages.hpp"
#include "server/LoginServer.hpp"
#include "../Context.hpp"
#include "../json/JsonBox.hpp"

namespace abcd {

static Status
loginPasswordDisk(std::shared_ptr<Login> &result,
                  LoginStore &store, const std::string &password)
{
    const auto LP = store.username() + password;

    AccountPaths paths;
    ABC_CHECK(store.paths(paths));

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
    ABC_CHECK(Login::createOffline(result, store, dataKey));
    return Status();
}

static Status
loginPasswordServer(std::shared_ptr<Login> &result,
                    LoginStore &store, const std::string &password,
                    AuthError &authError)
{
    const auto LP = store.username() + password;

    // Create passwordAuth:
    DataChunk passwordAuth;
    ABC_CHECK(usernameSnrp().hash(passwordAuth, LP));

    // Grab the login information from the server:
    AuthJson authJson;
    LoginReplyJson loginJson;
    ABC_CHECK(authJson.passwordSet(store, passwordAuth));
    ABC_CHECK(loginServerLogin(loginJson, authJson, &authError));

    // Unlock passwordBox:
    DataChunk passwordKey;
    DataChunk dataKey;
    ABC_CHECK(loginJson.passwordKeySnrp().hash(passwordKey, LP));
    ABC_CHECK(loginJson.passwordBox().decrypt(dataKey, passwordKey));

    // Create the Login object:
    ABC_CHECK(Login::createOnline(result, store, dataKey, loginJson));
    return Status();
}

Status
loginPassword(std::shared_ptr<Login> &result,
              LoginStore &store, const std::string &password,
              AuthError &authError)
{
    // Try the login both ways:
    if (!loginPasswordDisk(result, store, password))
        ABC_CHECK(loginPasswordServer(result, store, password, authError));

    return Status();
}

Status
loginPasswordSet(Login &login, const std::string &password)
{
    std::string LP = login.store.username() + password;

    // Create passwordBox:
    JsonSnrp passwordKeySnrp;
    DataChunk passwordKey;
    JsonBox passwordBox;
    ABC_CHECK(passwordKeySnrp.create());
    ABC_CHECK(passwordKeySnrp.hash(passwordKey, LP));
    ABC_CHECK(passwordBox.encrypt(login.dataKey(), passwordKey));

    // Create passwordAuth:
    DataChunk passwordAuth;
    JsonBox passwordAuthBox;
    ABC_CHECK(usernameSnrp().hash(passwordAuth, LP));
    ABC_CHECK(passwordAuthBox.encrypt(passwordAuth, login.dataKey()));

    // Change the server login:
    AuthJson authJson;
    ABC_CHECK(authJson.loginSet(login));
    ABC_CHECK(loginServerPasswordSet(authJson,
                                     passwordAuth, passwordKeySnrp,
                                     passwordBox, passwordAuthBox));

    // Change the in-memory login:
    ABC_CHECK(login.passwordAuthSet(passwordAuth));

    // Change the on-disk login:
    CarePackage carePackage;
    ABC_CHECK(carePackage.load(login.paths.carePackagePath()));
    ABC_CHECK(carePackage.passwordKeySnrpSet(passwordKeySnrp));
    ABC_CHECK(carePackage.save(login.paths.carePackagePath()));

    LoginPackage loginPackage;
    ABC_CHECK(loginPackage.load(login.paths.loginPackagePath()));
    ABC_CHECK(loginPackage.passwordBoxSet(passwordBox));
    ABC_CHECK(loginPackage.passwordAuthBoxSet(passwordAuthBox));
    ABC_CHECK(loginPackage.save(login.paths.loginPackagePath()));

    return Status();
}

Status
loginPasswordOk(bool &result, Login &login, const std::string &password)
{
    std::string LP = login.store.username() + password;

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
    ABC_CHECK(LoginStore::fixUsername(fixed, username));
    AccountPaths paths;
    ABC_CHECK(gContext->paths.accountDir(paths, fixed));

    LoginPackage loginPackage;
    ABC_CHECK(loginPackage.load(paths.loginPackagePath()));

    result = !!loginPackage.passwordBox().get();
    return Status();
}

} // namespace abcd
