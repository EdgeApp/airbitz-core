/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginRecovery.hpp"
#include "Lobby.hpp"
#include "Login.hpp"
#include "LoginDir.hpp"
#include "LoginServer.hpp"
#include "../json/JsonBox.hpp"
#include "../util/Util.hpp"

namespace abcd {

/**
 * Obtains the recovery questions for a user.
 *
 * @param pszRecoveryQuestions The returned questions. The caller frees this.
 */
tABC_CC ABC_LoginGetRQ(Lobby &lobby,
                       char **pszRecoveryQuestions,
                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_CarePackage *pCarePackage = NULL;
    AutoU08Buf L4;
    DataChunk questions;

    // Load CarePackage:
    ABC_CHECK_RET(ABC_LoginServerGetCarePackage(toU08Buf(lobby.authId()), &pCarePackage, pError));

    // Verify that the questions exist:
    ABC_CHECK_ASSERT(pCarePackage->ERQ, ABC_CC_NoRecoveryQuestions, "No recovery questions");

    // Create L4:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(toU08Buf(lobby.username()), pCarePackage->pSNRP4, &L4, pError));

    // Decrypt:
    if (JsonBox(json_incref(pCarePackage->ERQ)).decrypt(questions, U08Buf(L4)))
    {
        ABC_STRDUP(*pszRecoveryQuestions, toString(questions).c_str());
    }
    else
    {
        *pszRecoveryQuestions = NULL;
    }

exit:
    ABC_CarePackageFree(pCarePackage);

    return cc;
}

/**
 * Loads an existing login object, either from the server or from disk.
 * Uses recovery answers rather than a password.
 */
tABC_CC ABC_LoginRecovery(std::shared_ptr<Login> &result,
                          std::shared_ptr<Lobby> lobby,
                          const char *szRecoveryAnswers,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::unique_ptr<Login> login;
    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    tABC_U08Buf         LP1             = ABC_BUF_NULL; // Do not free
    AutoU08Buf          LRA;
    AutoU08Buf          LRA1;
    AutoU08Buf          LRA3;
    DataChunk dataKey;          // Unlocks the account

    // Get the CarePackage:
    ABC_CHECK_RET(ABC_LoginServerGetCarePackage(toU08Buf(lobby->authId()), &pCarePackage, pError));

    // LRA = L + RA:
    ABC_BUF_STRCAT(LRA, lobby->username().c_str(), szRecoveryAnswers);

    // Get the LoginPackage:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pCarePackage->pSNRP1, &LRA1, pError));
    ABC_CHECK_RET(ABC_LoginServerGetLoginPackage(toU08Buf(lobby->authId()), LP1, LRA1, &pLoginPackage, pError));

    // Decrypt MK:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pCarePackage->pSNRP3, &LRA3, pError));
    ABC_CHECK_NEW(JsonBox(json_incref(pLoginPackage->EMK_LRA3)).decrypt(dataKey, U08Buf(LRA3)), pError);

    // Decrypt SyncKey:
    login.reset(new Login(lobby, dataKey));
    ABC_CHECK_NEW(login->init(pLoginPackage), pError);

    // Set up the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirSavePackages(lobby->dir(), pCarePackage, pLoginPackage, pError));

    // Assign the final output:
    result.reset(login.release());

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

/**
 * Changes the recovery questions and answers on an existing login object.
 */
tABC_CC ABC_LoginRecoverySet(Login &login,
                             const char *szRecoveryQuestions,
                             const char *szRecoveryAnswers,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_CarePackage *pCarePackage = NULL;
    tABC_LoginPackage *pLoginPackage = NULL;
    AutoU08Buf oldL1;
    AutoU08Buf oldLP1;
    AutoU08Buf L4;
    AutoU08Buf RQ;
    AutoU08Buf LRA;
    AutoU08Buf LRA1;
    AutoU08Buf LRA3;
    JsonBox box;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(login.lobby().dir(), &pCarePackage, &pLoginPackage, pError));

    // Load the old keys:
    ABC_CHECK_RET(ABC_LoginGetServerKeys(login, &oldL1, &oldLP1, pError));

    // Update scrypt parameters:
    ABC_CryptoFreeSNRP(pCarePackage->pSNRP3);
    ABC_CryptoFreeSNRP(pCarePackage->pSNRP4);
    pCarePackage->pSNRP3 = nullptr;
    pCarePackage->pSNRP4 = nullptr;
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&pCarePackage->pSNRP3, pError));
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&pCarePackage->pSNRP4, pError));

    // L4  = Scrypt(L, SNRP4):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(toU08Buf(login.lobby().username()), pCarePackage->pSNRP4, &L4, pError));

    // Update ERQ:
    ABC_BUF_DUP_PTR(RQ, (unsigned char *)szRecoveryQuestions, strlen(szRecoveryQuestions) + 1);
    ABC_CHECK_NEW(box.encrypt(U08Buf(RQ), U08Buf(L4)), pError);
    if (pCarePackage->ERQ) json_decref(pCarePackage->ERQ);
    pCarePackage->ERQ = json_incref(box.get());

    // LRA = L + RA:
    ABC_BUF_STRCAT(LRA, login.lobby().username().c_str(), szRecoveryAnswers);

    // Update EMK_LRA3:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pCarePackage->pSNRP3, &LRA3, pError));
    ABC_CHECK_NEW(box.encrypt(login.dataKey(), U08Buf(LRA3)), pError);
    if (pLoginPackage->EMK_LRA3) json_decref(pLoginPackage->EMK_LRA3);
    pLoginPackage->EMK_LRA3 = json_incref(box.get());

    // Update ELRA1:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pCarePackage->pSNRP1, &LRA1, pError));
    ABC_CHECK_NEW(box.encrypt(U08Buf(LRA1), login.dataKey()), pError);
    if (pLoginPackage->ELRA1) json_decref(pLoginPackage->ELRA1);
    pLoginPackage->ELRA1 = json_incref(box.get());

    // Change the server login:
    ABC_CHECK_RET(ABC_LoginServerChangePassword(oldL1, oldLP1,
        oldLP1, LRA1, pCarePackage, pLoginPackage, pError));

    // Change the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirSavePackages(login.lobby().dir(), pCarePackage, pLoginPackage, pError));

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

} // namespace abcd
