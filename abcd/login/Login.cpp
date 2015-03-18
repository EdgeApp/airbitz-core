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
#include "../util/FileIO.hpp"
#include "../util/Sync.hpp"
#include "../util/Util.hpp"
#include <ctype.h>

namespace abcd {

#define ACCOUNT_MK_LENGTH 32

Login::Login(std::shared_ptr<Lobby> lobby, DataSlice dataKey):
    lobby_(std::move(lobby)),
    dataKey_(dataKey.begin(), dataKey.end())
{}

Status
Login::init(tABC_LoginPackage *package)
{
    AutoString syncKey;
    ABC_CHECK_OLD(ABC_LoginPackageGetSyncKey(package, toU08Buf(dataKey_), &syncKey.get(), &error));
    syncKey_ = syncKey.get();

    // Ensure that the directories are in place:
    ABC_CHECK(lobby_->dirCreate());
    ABC_CHECK(syncDirCreate());
    return Status();
}

std::string
Login::syncDir() const
{
    return lobby_->dir() + "sync/";
}

Status
Login::syncDirCreate() const
{
    // Locate the sync dir:
    bool exists = false;
    ABC_CHECK_OLD(ABC_FileIOFileExists(syncDir().c_str(), &exists, &error));

    // If it doesn't exist, create it:
    if (!exists)
    {
        int dirty = 0;
        std::string tempName = lobby_->dir() + "tmp/";
        ABC_CHECK_OLD(ABC_FileIOFileExists(tempName.c_str(), &exists, &error));
        if (exists)
            ABC_CHECK_OLD(ABC_FileIODeleteRecursive(tempName.c_str(), &error));
        ABC_CHECK_OLD(ABC_SyncMakeRepo(tempName.c_str(), &error));
        ABC_CHECK_OLD(ABC_SyncRepo(tempName.c_str(), syncKey_.c_str(), &dirty, &error));
        if (rename(tempName.c_str(), syncDir().c_str()))
            return ABC_ERROR(ABC_CC_SysError, "rename failed");
    }

    return Status();
}

/**
 * Creates a new login account, both on-disk and on the server.
 *
 * @param szUserName    The user name for the account.
 * @param szPassword    The password for the account.
 * @param ppSelf        The returned login object.
 */
tABC_CC ABC_LoginCreate(std::shared_ptr<Login> &result,
                        std::shared_ptr<Lobby> lobby,
                        const char *szPassword,
                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::unique_ptr<Login> login;
    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    AutoU08Buf           LP;
    AutoU08Buf           LP1;
    AutoU08Buf           LP2;
    DataChunk dataKey;
    DataChunk syncKey;

    // Set up packages:
    ABC_CHECK_RET(ABC_CarePackageNew(&pCarePackage, pError));
    ABC_NEW(pLoginPackage, tABC_LoginPackage);

    // Generate MK:
    ABC_CHECK_NEW(randomData(dataKey, ACCOUNT_MK_LENGTH), pError);

    // Generate SyncKey:
    ABC_CHECK_NEW(randomData(syncKey, SYNC_KEY_LENGTH), pError);

    // LP = L + P:
    ABC_BUF_STRCAT(LP, lobby->username().c_str(), szPassword);

    // Set up EMK_LP2:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(toU08Buf(dataKey), LP2,
        ABC_CryptoType_AES256, &pLoginPackage->EMK_LP2, pError));

    // Set up ESyncKey:
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(toU08Buf(syncKey), toU08Buf(dataKey),
        ABC_CryptoType_AES256, &pLoginPackage->ESyncKey, pError));

    // Set up ELP1:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP1, &LP1, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(LP1, toU08Buf(dataKey),
        ABC_CryptoType_AES256, &pLoginPackage->ELP1, pError));

    // Create the account and repo on server:
    ABC_CHECK_RET(ABC_LoginServerCreate(toU08Buf(lobby->authId()), LP1,
        pCarePackage, pLoginPackage, base16Encode(syncKey).c_str(), pError));

    // Latch the account:
    ABC_CHECK_RET(ABC_LoginServerActivate(toU08Buf(lobby->authId()), LP1, pError));

    // Set up the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirSavePackages(lobby->dir(), pCarePackage, pLoginPackage, pError));

    // Create the Login object:
    login.reset(new Login(lobby, dataKey));
    ABC_CHECK_NEW(login->init(pLoginPackage), pError);
    result.reset(login.release());

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);
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

    ABC_CHECK_RET(ABC_LoginDirLoadPackages(login.lobby().dir(), &pCarePackage, &pLoginPackage, pError));
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pLoginPackage->ELP1, toU08Buf(login.dataKey()), pLP1, pError));

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

} // namespace abcd
