/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginPassword.hpp"
#include "Lobby.hpp"
#include "Login.hpp"
#include "LoginDir.hpp"
#include "../auth/LoginServer.hpp"
#include "../json/JsonBox.hpp"
#include "../util/Util.hpp"

namespace abcd {

static
tABC_CC ABC_LoginPasswordDisk(std::shared_ptr<Login> &result,
                              Lobby &lobby,
                              const char *szPassword,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::shared_ptr<Login> out;
    CarePackage carePackage;
    LoginPackage loginPackage;
    DataChunk passwordKey;      // Unlocks dataKey
    DataChunk dataKey;          // Unlocks the account
    std::string LP = lobby.username() + szPassword;

    // Load the packages:
    ABC_CHECK_NEW(carePackage.load(lobby.carePackageName()));
    ABC_CHECK_NEW(loginPackage.load(lobby.loginPackageName()));

    // Decrypt MK:
    ABC_CHECK_NEW(carePackage.snrp2().hash(passwordKey, LP));
    ABC_CHECK_NEW(loginPackage.passwordBox().decrypt(dataKey, passwordKey));

    // Decrypt SyncKey:
    ABC_CHECK_NEW(Login::create(out, lobby, dataKey, loginPackage));
    result = std::move(out);

exit:
    return cc;
}

static
tABC_CC ABC_LoginPasswordServer(std::shared_ptr<Login> &result,
                                Lobby &lobby,
                                const char *szPassword,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::shared_ptr<Login> out;
    CarePackage carePackage;
    LoginPackage loginPackage;
    DataChunk authKey;          // Unlocks the server
    DataChunk passwordKey;      // Unlocks dataKey
    DataChunk dataKey;          // Unlocks the account
    std::string LP = lobby.username() + szPassword;

    // Get the CarePackage:
    ABC_CHECK_RET(ABC_LoginServerGetCarePackage(lobby, carePackage, pError));

    // Get the LoginPackage:
    ABC_CHECK_NEW(usernameSnrp().hash(authKey, LP));
    ABC_CHECK_RET(ABC_LoginServerGetLoginPackage(lobby,
        toU08Buf(authKey), U08Buf(), loginPackage, pError));

    // Decrypt MK:
    ABC_CHECK_NEW(carePackage.snrp2().hash(passwordKey, LP));
    ABC_CHECK_NEW(loginPackage.passwordBox().decrypt(dataKey, passwordKey));

    // Decrypt SyncKey:
    ABC_CHECK_NEW(Login::create(out, lobby, dataKey, loginPackage));

    // Set up the on-disk login:
    ABC_CHECK_NEW(carePackage.save(lobby.carePackageName()));
    ABC_CHECK_NEW(loginPackage.save(lobby.loginPackageName()));

    // Assign the result:
    result = std::move(out);

exit:
    return cc;
}

/**
 * Loads an existing login object, either from the server or from disk.
 *
 * @param szPassword    The password for the account.
 */
tABC_CC ABC_LoginPassword(std::shared_ptr<Login> &result,
                          Lobby &lobby,
                          const char *szPassword,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Error error;

    // Try the login both ways:
    cc = ABC_LoginPasswordDisk(result, lobby, szPassword, &error);
    if (ABC_CC_Ok != cc)
    {
        ABC_CHECK_RET(ABC_LoginPasswordServer(result, lobby, szPassword, pError));
    }

exit:
    return cc;
}

/**
 * Changes the password on an existing login object.
 * @param szPassword    The new password.
 */
tABC_CC ABC_LoginPasswordSet(Login &login,
                             const char *szPassword,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    CarePackage carePackage;
    LoginPackage loginPackage;
    AutoU08Buf oldLP1;
    DataChunk oldLRA1;
    DataChunk authKey;          // Unlocks the server
    DataChunk passwordKey;      // Unlocks dataKey
    JsonBox box;
    JsonSnrp snrp;
    std::string LP = login.lobby.username() + szPassword;

    // Load the packages:
    ABC_CHECK_NEW(carePackage.load(login.lobby.carePackageName()));
    ABC_CHECK_NEW(loginPackage.load(login.lobby.loginPackageName()));

    // Load the old keys:
    ABC_CHECK_RET(ABC_LoginGetServerKey(login, &oldLP1, pError));
    if (loginPackage.ELRA1())
    {
        ABC_CHECK_NEW(loginPackage.ELRA1().decrypt(oldLRA1, login.dataKey()));
    }

    // Update SNRP2:
    ABC_CHECK_NEW(snrp.create());
    ABC_CHECK_NEW(carePackage.snrp2Set(snrp));

    // Update EMK_LP2:
    ABC_CHECK_NEW(carePackage.snrp2().hash(passwordKey, LP));
    ABC_CHECK_NEW(box.encrypt(login.dataKey(), passwordKey));
    ABC_CHECK_NEW(loginPackage.passwordBoxSet(box));

    // Update ELP1:
    ABC_CHECK_NEW(usernameSnrp().hash(authKey, LP));
    ABC_CHECK_NEW(box.encrypt(authKey, login.dataKey()));
    ABC_CHECK_NEW(loginPackage.authKeyBoxSet(box));

    // Change the server login:
    ABC_CHECK_RET(ABC_LoginServerChangePassword(login.lobby, oldLP1,
        toU08Buf(authKey), toU08Buf(oldLRA1), carePackage, loginPackage, pError));

    // Change the on-disk login:
    ABC_CHECK_NEW(carePackage.save(login.lobby.carePackageName()));
    ABC_CHECK_NEW(loginPackage.save(login.lobby.loginPackageName()));

exit:
    return cc;
}

/**
 * Validates that the provided password is correct.
 * This is used in the GUI to guard access to certain actions.
 *
 * @param szPassword    The password to check.
 * @param pOk           Set to true if the password is good, or false otherwise.
 */
tABC_CC ABC_LoginPasswordOk(Login &login,
                            const char *szPassword,
                            bool *pOk,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    CarePackage carePackage;
    LoginPackage loginPackage;
    DataChunk passwordKey;      // Unlocks dataKey
    DataChunk dataKey;          // Unlocks the account
    std::string LP = login.lobby.username() + szPassword;

    *pOk = false;

    // Load the packages:
    ABC_CHECK_NEW(carePackage.load(login.lobby.carePackageName()));
    ABC_CHECK_NEW(loginPackage.load(login.lobby.loginPackageName()));

    // Try to decrypt MK:
    ABC_CHECK_NEW(carePackage.snrp2().hash(passwordKey, LP));
    if (loginPackage.passwordBox().decrypt(dataKey, passwordKey))
    {
        *pOk = true;
    }

exit:
    return cc;
}

Status
passwordExists(bool &result, Login &login)
{
    LoginPackage loginPackage;
    ABC_CHECK(loginPackage.load(login.lobby.loginPackageName()));

    result = !!loginPackage.passwordBox().get();
    return Status();
}

} // namespace abcd
