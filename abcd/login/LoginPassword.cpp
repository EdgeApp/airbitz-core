/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginPassword.hpp"
#include "Lobby.hpp"
#include "LoginDir.hpp"
#include "LoginServer.hpp"
#include "../crypto/Crypto.hpp"
#include "../util/Util.hpp"
#include <memory>

namespace abcd {

static
tABC_CC ABC_LoginPasswordDisk(Login *&result,
                              Lobby *lobby,
                              tABC_U08Buf LP,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::unique_ptr<Login> login;
    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    AutoU08Buf          LP2;
    AutoU08Buf          MK;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(lobby->directory(), &pCarePackage, &pLoginPackage, pError));

    // Decrypt MK:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pLoginPackage->EMK_LP2, LP2, &MK, pError));

    // Decrypt SyncKey:
    login.reset(new Login(lobby, static_cast<U08Buf>(MK)));
    ABC_CHECK_NEW(login->init(pLoginPackage), pError);
    result = login.release();

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

static
tABC_CC ABC_LoginPasswordServer(Login *&result,
                                Lobby *lobby,
                                tABC_U08Buf LP,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::unique_ptr<Login> login;
    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    tABC_U08Buf         LRA1            = ABC_BUF_NULL; // Do not free
    AutoU08Buf          LP1;
    AutoU08Buf          LP2;
    AutoU08Buf          MK;

    // Get the CarePackage:
    ABC_CHECK_RET(ABC_LoginServerGetCarePackage(toU08Buf(lobby->authId()), &pCarePackage, pError));

    // Get the LoginPackage:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP1, &LP1, pError));
    ABC_CHECK_RET(ABC_LoginServerGetLoginPackage(toU08Buf(lobby->authId()), LP1, LRA1, &pLoginPackage, pError));

    // Decrypt MK:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pLoginPackage->EMK_LP2, LP2, &MK, pError));

    // Decrypt SyncKey:
    login.reset(new Login(lobby, static_cast<U08Buf>(MK)));
    ABC_CHECK_NEW(login->init(pLoginPackage), pError);

    // Set up the on-disk login:
    ABC_CHECK_NEW(lobby->createDirectory(), pError);
    ABC_CHECK_RET(ABC_LoginDirSavePackages(lobby->directory(), pCarePackage, pLoginPackage, pError));

    // Assign the result:
    result = login.release();

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

/**
 * Loads an existing login object, either from the server or from disk.
 *
 * @param szPassword    The password for the account.
 */
tABC_CC ABC_LoginPassword(Login *&result,
                          Lobby *lobby,
                          const char *szPassword,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Error error;

    // LP = L + P:
    AutoU08Buf LP;
    ABC_BUF_STRCAT(LP, lobby->username().c_str(), szPassword);

    // Try the login both ways:
    cc = ABC_LoginPasswordDisk(result, lobby, LP, &error);
    if (ABC_CC_Ok != cc)
    {
        ABC_CHECK_RET(ABC_LoginPasswordServer(result, lobby, LP, pError));
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

    tABC_CarePackage *pCarePackage = NULL;
    tABC_LoginPackage *pLoginPackage = NULL;
    AutoU08Buf oldL1;
    AutoU08Buf oldLP1;
    AutoU08Buf oldLRA1;
    AutoU08Buf LP;
    AutoU08Buf LP1;
    AutoU08Buf LP2;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(login.lobby().directory(), &pCarePackage, &pLoginPackage, pError));

    // Load the old keys:
    ABC_CHECK_RET(ABC_LoginGetServerKeys(login, &oldL1, &oldLP1, pError));
    if (pLoginPackage->ELRA1)
    {
        ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pLoginPackage->ELRA1, toU08Buf(login.mk()), &oldLRA1, pError));
    }

    // Update SNRP2:
    ABC_CryptoFreeSNRP(pCarePackage->pSNRP2);
    pCarePackage->pSNRP2 = nullptr;
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&pCarePackage->pSNRP2, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, login.lobby().username().c_str(), szPassword);

    // Update EMK_LP2:
    json_decref(pLoginPackage->EMK_LP2);
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(toU08Buf(login.mk()), LP2,
        ABC_CryptoType_AES256, &pLoginPackage->EMK_LP2, pError));

    // Update ELP1:
    json_decref(pLoginPackage->ELP1);
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP1, &LP1, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(LP1, toU08Buf(login.mk()),
        ABC_CryptoType_AES256, &pLoginPackage->ELP1, pError));

    // Change the server login:
    ABC_CHECK_RET(ABC_LoginServerChangePassword(oldL1, oldLP1,
        LP1, oldLRA1, pCarePackage, pLoginPackage, pError));

    // Change the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirSavePackages(login.lobby().directory(), pCarePackage, pLoginPackage, pError));

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

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

    tABC_CarePackage *pCarePackage = NULL;
    tABC_LoginPackage *pLoginPackage = NULL;
    AutoU08Buf LP;
    AutoU08Buf LP2;
    AutoU08Buf MK;

    *pOk = false;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(login.lobby().directory(), &pCarePackage, &pLoginPackage, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, login.lobby().username().c_str(), szPassword);
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));

    // Try to decrypt MK:
    tABC_Error error;
    if (ABC_CC_Ok == ABC_CryptoDecryptJSONObject(pLoginPackage->EMK_LP2, LP2, &MK, &error))
    {
        *pOk = true;
    }

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

} // namespace abcd
