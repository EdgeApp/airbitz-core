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
#include "LoginServer.hpp"
#include "../json/JsonBox.hpp"
#include "../util/Util.hpp"

namespace abcd {

static
tABC_CC ABC_LoginPasswordDisk(std::shared_ptr<Login> &result,
                              std::shared_ptr<Lobby> lobby,
                              const char *szPassword,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::unique_ptr<Login> login;
    CarePackage carePackage;
    LoginPackage loginPackage;
    DataChunk passwordKey;      // Unlocks dataKey
    DataChunk dataKey;          // Unlocks the account
    std::string LP = lobby->username() + szPassword;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(lobby->dir(), carePackage, loginPackage, pError));

    // Decrypt MK:
    ABC_CHECK_NEW(carePackage.snrp2().hash(passwordKey, LP), pError);
    ABC_CHECK_NEW(loginPackage.passwordBox().decrypt(dataKey, passwordKey), pError);

    // Decrypt SyncKey:
    login.reset(new Login(lobby, dataKey));
    ABC_CHECK_NEW(login->init(loginPackage), pError);
    result.reset(login.release());

exit:
    return cc;
}

static
tABC_CC ABC_LoginPasswordServer(std::shared_ptr<Login> &result,
                                std::shared_ptr<Lobby> lobby,
                                const char *szPassword,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::unique_ptr<Login> login;
    CarePackage carePackage;
    LoginPackage loginPackage;
    DataChunk authKey;          // Unlocks the server
    DataChunk passwordKey;      // Unlocks dataKey
    DataChunk dataKey;          // Unlocks the account
    std::string LP = lobby->username() + szPassword;

    // Get the CarePackage:
    ABC_CHECK_RET(ABC_LoginServerGetCarePackage(toU08Buf(lobby->authId()), carePackage, pError));

    // Get the LoginPackage:
    ABC_CHECK_NEW(usernameSnrp().hash(authKey, LP), pError);
    ABC_CHECK_RET(ABC_LoginServerGetLoginPackage(
        toU08Buf(lobby->authId()), toU08Buf(authKey), U08Buf(),
        loginPackage, pError));

    // Decrypt MK:
    ABC_CHECK_NEW(carePackage.snrp2().hash(passwordKey, LP), pError);
    ABC_CHECK_NEW(loginPackage.passwordBox().decrypt(dataKey, passwordKey), pError);

    // Decrypt SyncKey:
    login.reset(new Login(lobby, dataKey));
    ABC_CHECK_NEW(login->init(loginPackage), pError);

    // Set up the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirSavePackages(lobby->dir(), carePackage, loginPackage, pError));

    // Assign the result:
    result.reset(login.release());

exit:
    return cc;
}

/**
 * Loads an existing login object, either from the server or from disk.
 *
 * @param szPassword    The password for the account.
 */
tABC_CC ABC_LoginPassword(std::shared_ptr<Login> &result,
                          std::shared_ptr<Lobby> lobby,
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
    AutoU08Buf oldL1;
    AutoU08Buf oldLP1;
    DataChunk oldLRA1;
    DataChunk authKey;          // Unlocks the server
    DataChunk passwordKey;      // Unlocks dataKey
    JsonBox box;
    JsonSnrp snrp;
    std::string LP = login.lobby().username() + szPassword;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(login.lobby().dir(), carePackage, loginPackage, pError));

    // Load the old keys:
    ABC_CHECK_RET(ABC_LoginGetServerKeys(login, &oldL1, &oldLP1, pError));
    if (loginPackage.ELRA1())
    {
        ABC_CHECK_NEW(loginPackage.ELRA1().decrypt(oldLRA1, login.dataKey()), pError);
    }

    // Update SNRP2:
    ABC_CHECK_NEW(snrp.create(), pError);
    ABC_CHECK_NEW(carePackage.snrp2Set(snrp), pError);

    // Update EMK_LP2:
    ABC_CHECK_NEW(carePackage.snrp2().hash(passwordKey, LP), pError);
    ABC_CHECK_NEW(box.encrypt(login.dataKey(), passwordKey), pError);
    ABC_CHECK_NEW(loginPackage.passwordBoxSet(box), pError);

    // Update ELP1:
    ABC_CHECK_NEW(usernameSnrp().hash(authKey, LP), pError);
    ABC_CHECK_NEW(box.encrypt(authKey, login.dataKey()), pError);
    ABC_CHECK_NEW(loginPackage.authKeyBoxSet(box), pError);

    // Change the server login:
    ABC_CHECK_RET(ABC_LoginServerChangePassword(oldL1, oldLP1,
        toU08Buf(authKey), toU08Buf(oldLRA1), carePackage, loginPackage, pError));

    // Change the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirSavePackages(login.lobby().dir(), carePackage, loginPackage, pError));

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
    std::string LP = login.lobby().username() + szPassword;

    *pOk = false;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(login.lobby().dir(), carePackage, loginPackage, pError));

    // Try to decrypt MK:
    ABC_CHECK_NEW(carePackage.snrp2().hash(passwordKey, LP), pError);
    if (loginPackage.passwordBox().decrypt(dataKey, passwordKey))
    {
        *pOk = true;
    }

exit:
    return cc;
}

} // namespace abcd
