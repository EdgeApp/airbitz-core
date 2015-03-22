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

    CarePackage carePackage;
    DataChunk questionKey;      // Unlocks questions
    DataChunk questions;

    // Load CarePackage:
    ABC_CHECK_RET(ABC_LoginServerGetCarePackage(toU08Buf(lobby.authId()), carePackage, pError));

    // Verify that the questions exist:
    ABC_CHECK_ASSERT(carePackage.questionBox(), ABC_CC_NoRecoveryQuestions, "No recovery questions");

    // Create L4:
    ABC_CHECK_NEW(carePackage.snrp4().hash(questionKey, lobby.username()), pError);

    // Decrypt:
    if (carePackage.questionBox().decrypt(questions, questionKey))
    {
        ABC_STRDUP(*pszRecoveryQuestions, toString(questions).c_str());
    }
    else
    {
        *pszRecoveryQuestions = NULL;
    }

exit:
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
    CarePackage carePackage;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    tABC_U08Buf         LP1             = ABC_BUF_NULL; // Do not free
    DataChunk recoveryAuthKey;  // Unlocks the server
    DataChunk recoveryKey;      // Unlocks dataKey
    DataChunk dataKey;          // Unlocks the account
    std::string LRA = lobby->username() + szRecoveryAnswers;

    // Get the CarePackage:
    ABC_CHECK_RET(ABC_LoginServerGetCarePackage(toU08Buf(lobby->authId()), carePackage, pError));

    // Get the LoginPackage:
    ABC_CHECK_NEW(usernameSnrp().hash(recoveryAuthKey, LRA), pError);
    ABC_CHECK_RET(ABC_LoginServerGetLoginPackage(
        toU08Buf(lobby->authId()), LP1, toU08Buf(recoveryAuthKey),
        &pLoginPackage, pError));

    // Decrypt MK:
    ABC_CHECK_NEW(carePackage.snrp3().hash(recoveryKey, LRA), pError);
    ABC_CHECK_NEW(JsonBox(json_incref(pLoginPackage->EMK_LRA3)).decrypt(dataKey, recoveryKey), pError);

    // Decrypt SyncKey:
    login.reset(new Login(lobby, dataKey));
    ABC_CHECK_NEW(login->init(pLoginPackage), pError);

    // Set up the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirSavePackages(lobby->dir(), carePackage, pLoginPackage, pError));

    // Assign the final output:
    result.reset(login.release());

exit:
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

    CarePackage carePackage;
    tABC_LoginPackage *pLoginPackage = NULL;
    AutoU08Buf oldL1;
    AutoU08Buf oldLP1;
    AutoU08Buf RQ;
    DataChunk questionKey;      // Unlocks questions
    DataChunk recoveryAuthKey;  // Unlocks the server
    DataChunk recoveryKey;      // Unlocks dataKey
    JsonBox box;
    JsonSnrp snrp;
    std::string LRA = login.lobby().username() + szRecoveryAnswers;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(login.lobby().dir(), carePackage, &pLoginPackage, pError));

    // Load the old keys:
    ABC_CHECK_RET(ABC_LoginGetServerKeys(login, &oldL1, &oldLP1, pError));

    // Update scrypt parameters:
    ABC_CHECK_NEW(snrp.create(), pError);
    ABC_CHECK_NEW(carePackage.snrp3Set(snrp), pError);
    ABC_CHECK_NEW(snrp.create(), pError);
    ABC_CHECK_NEW(carePackage.snrp4Set(snrp), pError);

    // L4  = Scrypt(L, SNRP4):
    ABC_CHECK_NEW(carePackage.snrp4().hash(questionKey, login.lobby().username()), pError);

    // Update ERQ:
    ABC_BUF_DUP_PTR(RQ, (unsigned char *)szRecoveryQuestions, strlen(szRecoveryQuestions) + 1);
    ABC_CHECK_NEW(box.encrypt(U08Buf(RQ), questionKey), pError);
    ABC_CHECK_NEW(carePackage.questionBoxSet(box), pError);

    // Update EMK_LRA3:
    ABC_CHECK_NEW(carePackage.snrp3().hash(recoveryKey, LRA), pError);
    ABC_CHECK_NEW(box.encrypt(login.dataKey(), recoveryKey), pError);
    if (pLoginPackage->EMK_LRA3) json_decref(pLoginPackage->EMK_LRA3);
    pLoginPackage->EMK_LRA3 = json_incref(box.get());

    // Update ELRA1:
    ABC_CHECK_NEW(usernameSnrp().hash(recoveryAuthKey, LRA), pError);
    ABC_CHECK_NEW(box.encrypt(recoveryAuthKey, login.dataKey()), pError);
    if (pLoginPackage->ELRA1) json_decref(pLoginPackage->ELRA1);
    pLoginPackage->ELRA1 = json_incref(box.get());

    // Change the server login:
    ABC_CHECK_RET(ABC_LoginServerChangePassword(oldL1, oldLP1,
        oldLP1, toU08Buf(recoveryAuthKey), carePackage, pLoginPackage, pError));

    // Change the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirSavePackages(login.lobby().dir(), carePackage, pLoginPackage, pError));

exit:
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

} // namespace abcd
