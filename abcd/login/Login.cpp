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
#include "../crypto/Crypto.hpp"
#include "../crypto/Encoding.hpp"
#include "../crypto/Random.hpp"
#include "../util/Util.hpp"
#include <ctype.h>
#include <memory>

namespace abcd {

#define ACCOUNT_MK_LENGTH 32

Login::Login(Lobby *lobby, DataSlice mk):
    lobby_(lobby),
    mk_(mk.begin(), mk.end())
{}

Status
Login::init(tABC_LoginPackage *package)
{
    AutoString syncKey;
    ABC_CHECK_OLD(ABC_LoginPackageGetSyncKey(package, toU08Buf(mk_), &syncKey.get(), &error));
    syncKey_ = syncKey.get();
    return Status();
}

/**
 * Creates a new login account, both on-disk and on the server.
 *
 * @param szUserName    The user name for the account.
 * @param szPassword    The password for the account.
 * @param ppSelf        The returned login object.
 */
tABC_CC ABC_LoginCreate(Login *&result,
                        Lobby *lobby,
                        const char *szPassword,
                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::unique_ptr<Login> login;
    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    AutoU08Buf           MK;
    AutoU08Buf           SyncKey;
    AutoU08Buf           LP;
    AutoU08Buf           LP1;
    AutoU08Buf           LP2;

    // Set up packages:
    ABC_CHECK_RET(ABC_CarePackageNew(&pCarePackage, pError));
    ABC_NEW(pLoginPackage, tABC_LoginPackage);

    // Generate MK:
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(ACCOUNT_MK_LENGTH, &MK, pError));

    // Generate SyncKey:
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(SYNC_KEY_LENGTH, &SyncKey, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, lobby->username().c_str(), szPassword);

    // Set up EMK_LP2:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(MK, LP2,
        ABC_CryptoType_AES256, &pLoginPackage->EMK_LP2, pError));

    // Set up ESyncKey:
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(SyncKey, MK,
        ABC_CryptoType_AES256, &pLoginPackage->ESyncKey, pError));

    // Set up ELP1:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP1, &LP1, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(LP1, MK,
        ABC_CryptoType_AES256, &pLoginPackage->ELP1, pError));

    // Create the account and repo on server:
    ABC_CHECK_RET(ABC_LoginServerCreate(toU08Buf(lobby->authId()), LP1,
        pCarePackage, pLoginPackage, base16Encode(U08Buf(SyncKey)).c_str(), pError));

    // Latch the account:
    ABC_CHECK_RET(ABC_LoginServerActivate(toU08Buf(lobby->authId()), LP1, pError));

    // Set up the on-disk login:
    ABC_CHECK_NEW(lobby->createDirectory(), pError);
    ABC_CHECK_RET(ABC_LoginDirSavePackages(lobby->directory(), pCarePackage, pLoginPackage, pError));

    // Create the Login object:
    login.reset(new Login(lobby, static_cast<U08Buf>(MK)));
    ABC_CHECK_NEW(login->init(pLoginPackage), pError);
    result = login.release();

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);
    return cc;
}

/**
 * Obtains the sync keys for accessing an account's repo.
 * @param ppKeys    The returned keys. Call ABC_SyncFreeKeys when done.
 */
tABC_CC ABC_LoginGetSyncKeys(Login &login,
                             tABC_SyncKeys **ppKeys,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoSyncKeys pKeys;

    ABC_NEW(pKeys.get(), tABC_SyncKeys);
    ABC_CHECK_RET(ABC_LoginDirGetSyncDir(login.lobby().directory(), &pKeys->szSyncDir, pError));
    ABC_BUF_DUP(pKeys->MK, toU08Buf(login.mk()));
    ABC_STRDUP(pKeys->szSyncKey, login.syncKey().c_str());

    *ppKeys = pKeys;
    pKeys.get() = NULL;

exit:
    return cc;
}

/**
 * Obtains an account object's user name.
 * @param pL1       The hashed user name. The caller must free this.
 * @param pLP1      The hashed user name & password. The caller must free this.
 */
tABC_CC ABC_LoginGetServerKeys(Login &login,
                               tABC_U08Buf *pL1,
                               tABC_U08Buf *pLP1,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_CarePackage *pCarePackage = NULL;
    tABC_LoginPackage *pLoginPackage = NULL;

    ABC_BUF_DUP(*pL1, toU08Buf(login.lobby().authId()));

    ABC_CHECK_RET(ABC_LoginDirLoadPackages(login.lobby().directory(), &pCarePackage, &pLoginPackage, pError));
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pLoginPackage->ELP1, toU08Buf(login.mk()), pLP1, pError));

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

} // namespace abcd
