/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Login.hpp"
#include "Lobby.hpp"
#include "LoginDir.hpp"
#include "LoginServer.hpp"
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
    DataChunk syncKey;
    ABC_CHECK(loginPackage.syncKeyBox().decrypt(syncKey, dataKey));
    ABC_CHECK(lobby.dirCreate());

    DataChunk rootKey;
    if (fileExists(lobby.dir() + "tempRootKey.json"))
    {
        JsonBox box;
        ABC_CHECK(box.load(lobby.dir() + "tempRootKey.json"));
        ABC_CHECK(box.decrypt(rootKey, dataKey));
    }
    else
    {
        JsonBox box;
        ABC_CHECK(randomData(rootKey, 256));
        ABC_CHECK(box.encrypt(rootKey, dataKey));
        ABC_CHECK(box.save(lobby.dir() + "tempRootKey.json"));
    }

    result.reset(new Login(lobby, rootKey, dataKey, base16Encode(syncKey)));
    return Status();
}

Login::Login(Lobby &lobby, DataSlice rootKey, DataSlice dataKey, std::string syncKey):
    lobby(lobby),
    parent_(lobby.shared_from_this()),
    rootKey_(rootKey.begin(), rootKey.end()),
    dataKey_(dataKey.begin(), dataKey.end()),
    syncKey_(syncKey)
{}

/**
 * Creates a new login account, both on-disk and on the server.
 *
 * @param szUserName    The user name for the account.
 * @param szPassword    The password for the account.
 * @param ppSelf        The returned login object.
 */
tABC_CC ABC_LoginCreate(std::shared_ptr<Login> &result,
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
    DataChunk syncKey;
    JsonBox box;
    JsonSnrp snrp;

    // Generate SNRP2:
    ABC_CHECK_NEW(snrp.create());
    ABC_CHECK_NEW(carePackage.snrp2Set(snrp));

    // Generate MK:
    ABC_CHECK_NEW(randomData(dataKey, DATA_KEY_LENGTH));

    // Generate SyncKey:
    ABC_CHECK_NEW(randomData(syncKey, SYNC_KEY_LENGTH));

    if (szPassword)
    {
        std::string LP = lobby.username() + szPassword;

        // Generate authKey:
        ABC_CHECK_NEW(usernameSnrp().hash(authKey, LP));

        // Set up EMK_LP2:
        ABC_CHECK_NEW(carePackage.snrp2().hash(passwordKey, LP));
        ABC_CHECK_NEW(box.encrypt(dataKey, passwordKey));
        ABC_CHECK_NEW(loginPackage.passwordBoxSet(box));
    }
    else
    {
        // Generate authKey:
        ABC_CHECK_NEW(randomData(authKey, scryptDefaultSize));
    }

    // Encrypt authKey:
    ABC_CHECK_NEW(box.encrypt(authKey, dataKey));
    ABC_CHECK_NEW(loginPackage.authKeyBoxSet(box));

    // Set up ESyncKey:
    ABC_CHECK_NEW(box.encrypt(syncKey, dataKey));
    ABC_CHECK_NEW(loginPackage.syncKeyBoxSet(box));

    // Create the account and repo on server:
    ABC_CHECK_RET(ABC_LoginServerCreate(lobby, toU08Buf(authKey),
        carePackage, loginPackage, base16Encode(syncKey).c_str(), pError));

    // Create the Login object:
    ABC_CHECK_NEW(Login::create(out, lobby, dataKey, loginPackage));

    // Latch the account:
    ABC_CHECK_RET(ABC_LoginServerActivate(lobby, toU08Buf(authKey), pError));

    // Set up the on-disk login:
    ABC_CHECK_NEW(carePackage.save(lobby.carePackageName()));
    ABC_CHECK_NEW(loginPackage.save(lobby.loginPackageName()));

    // Assign the result:
    result = std::move(out);

exit:
    return cc;
}

/**
 * Obtains the account's server authentication key.
 * @param pLP1      The hashed user name & password. The caller must free this.
 */
tABC_CC ABC_LoginGetServerKey(const Login &login,
                               tABC_U08Buf *pLP1,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    LoginPackage loginPackage;
    DataChunk authKey;          // Unlocks the server

    ABC_CHECK_NEW(loginPackage.load(login.lobby.loginPackageName()));
    ABC_CHECK_NEW(loginPackage.authKeyBox().decrypt(authKey, login.dataKey()));
    ABC_BUF_DUP(*pLP1, toU08Buf(authKey));

exit:
    return cc;
}

} // namespace abcd
