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
#include "LoginPackages.hpp"
#include "../auth/LoginServer.hpp"
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
    ABC_CHECK_NEW(loginServerGetCarePackage(lobby, carePackage));

    // Verify that the questions exist:
    ABC_CHECK_ASSERT(carePackage.questionBox(), ABC_CC_NoRecoveryQuestions, "No recovery questions");

    // Create L4:
    ABC_CHECK_NEW(carePackage.snrp4().hash(questionKey, lobby.username()));

    // Decrypt:
    if (carePackage.questionBox().decrypt(questions, questionKey))
    {
        *pszRecoveryQuestions = stringCopy(toString(questions));
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
                          Lobby &lobby,
                          const char *szRecoveryAnswers,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::shared_ptr<Login> out;
    CarePackage carePackage;
    LoginPackage loginPackage;
    JsonPtr rootKeyBox;
    DataChunk recoveryAuthKey;  // Unlocks the server
    DataChunk recoveryKey;      // Unlocks dataKey
    DataChunk dataKey;          // Unlocks the account
    std::string LRA = lobby.username() + szRecoveryAnswers;

    // Get the CarePackage:
    ABC_CHECK_NEW(loginServerGetCarePackage(lobby, carePackage));

    // Get the LoginPackage:
    ABC_CHECK_NEW(usernameSnrp().hash(recoveryAuthKey, LRA));
    ABC_CHECK_NEW(loginServerGetLoginPackage(lobby,
        U08Buf(), recoveryAuthKey, loginPackage, rootKeyBox));

    // Decrypt MK:
    ABC_CHECK_NEW(carePackage.snrp3().hash(recoveryKey, LRA));
    ABC_CHECK_NEW(loginPackage.recoveryBox().decrypt(dataKey, recoveryKey));

    // Decrypt SyncKey:
    ABC_CHECK_NEW(Login::create(out, lobby, dataKey, loginPackage, rootKeyBox, false));

    // Set up the on-disk login:
    ABC_CHECK_NEW(carePackage.save(lobby.carePackageName()));
    ABC_CHECK_NEW(loginPackage.save(lobby.loginPackageName()));

    // Assign the final output:
    result = std::move(out);

exit:
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
    LoginPackage loginPackage;
    DataChunk authKey;          // Unlocks the server
    DataChunk questionKey;      // Unlocks questions
    DataChunk recoveryAuthKey;  // Unlocks the server
    DataChunk recoveryKey;      // Unlocks dataKey
    JsonBox box;
    JsonSnrp snrp;
    std::string LRA = login.lobby.username() + szRecoveryAnswers;

    // Load the packages:
    ABC_CHECK_NEW(carePackage.load(login.lobby.carePackageName()));
    ABC_CHECK_NEW(loginPackage.load(login.lobby.loginPackageName()));

    // Load the old keys:
    authKey = login.authKey();

    // Update scrypt parameters:
    ABC_CHECK_NEW(snrp.create());
    ABC_CHECK_NEW(carePackage.snrp3Set(snrp));
    ABC_CHECK_NEW(snrp.create());
    ABC_CHECK_NEW(carePackage.snrp4Set(snrp));

    // L4  = Scrypt(L, SNRP4):
    ABC_CHECK_NEW(carePackage.snrp4().hash(questionKey, login.lobby.username()));

    // Update ERQ:
    ABC_CHECK_NEW(box.encrypt(std::string(szRecoveryQuestions), questionKey));
    ABC_CHECK_NEW(carePackage.questionBoxSet(box));

    // Update EMK_LRA3:
    ABC_CHECK_NEW(carePackage.snrp3().hash(recoveryKey, LRA));
    ABC_CHECK_NEW(box.encrypt(login.dataKey(), recoveryKey));
    ABC_CHECK_NEW(loginPackage.recoveryBoxSet(box));

    // Update ELRA1:
    ABC_CHECK_NEW(usernameSnrp().hash(recoveryAuthKey, LRA));
    ABC_CHECK_NEW(box.encrypt(recoveryAuthKey, login.dataKey()));
    ABC_CHECK_NEW(loginPackage.ELRA1Set(box));

    // Change the server login:
    ABC_CHECK_NEW(loginServerChangePassword(login,
        authKey, recoveryAuthKey, carePackage, loginPackage));

    // Change the on-disk login:
    ABC_CHECK_NEW(carePackage.save(login.lobby.carePackageName()));
    ABC_CHECK_NEW(loginPackage.save(login.lobby.loginPackageName()));

exit:
    return cc;
}

} // namespace abcd
