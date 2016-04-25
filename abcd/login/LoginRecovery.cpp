/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginRecovery.hpp"
#include "Lobby.hpp"
#include "Login.hpp"
#include "LoginPackages.hpp"
#include "../auth/LoginServer.hpp"
#include "../json/JsonBox.hpp"

namespace abcd {

Status
loginRecoveryQuestions(std::string &result, Lobby &lobby)
{
    // Load CarePackage:
    CarePackage carePackage;
    ABC_CHECK(loginServerGetCarePackage(lobby, carePackage));

    // Verify that the questions exist:
    if (!carePackage.questionBox())
        return ABC_ERROR(ABC_CC_NoRecoveryQuestions, "No recovery questions");

    // Create questionKey (unlocks questions):
    DataChunk questionKey;
    ABC_CHECK(carePackage.snrp4().hash(questionKey, lobby.username()));

    // Decrypt:
    DataChunk questions;
    ABC_CHECK(carePackage.questionBox().decrypt(questions, questionKey));

    result = toString(questions);
    return Status();
}

Status
loginRecovery(std::shared_ptr<Login> &result,
              Lobby &lobby, const std::string &recoveryAnswers,
              AuthError &authError)
{
    std::string LRA = lobby.username() + recoveryAnswers;

    // Get the CarePackage:
    CarePackage carePackage;
    ABC_CHECK(loginServerGetCarePackage(lobby, carePackage));

    // Make recoveryAuthKey (unlocks the server):
    DataChunk recoveryAuthKey;
    ABC_CHECK(usernameSnrp().hash(recoveryAuthKey, LRA));

    // Get the LoginPackage:
    LoginPackage loginPackage;
    JsonPtr rootKeyBox;
    ABC_CHECK(loginServerGetLoginPackage(lobby, DataSlice(), recoveryAuthKey,
                                         loginPackage, rootKeyBox,
                                         authError));

    // Make recoveryKey (unlocks dataKey):
    DataChunk recoveryKey;
    ABC_CHECK(carePackage.snrp3().hash(recoveryKey, LRA));

    // Decrypt dataKey (unlocks the account):
    DataChunk dataKey;
    ABC_CHECK(loginPackage.recoveryBox().decrypt(dataKey, recoveryKey));

    // Create the Login object:
    std::shared_ptr<Login> out;
    ABC_CHECK(Login::create(out, lobby, dataKey,
                            loginPackage, rootKeyBox, false));

    // Set up the on-disk login:
    ABC_CHECK(carePackage.save(out->paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(out->paths.loginPackagePath()));

    result = std::move(out);
    return Status();
}

Status
loginRecoverySet(Login &login,
                 const std::string &recoveryQuestions,
                 const std::string &recoveryAnswers)
{
    std::string LRA = login.lobby.username() + recoveryAnswers;

    // Load the packages:
    CarePackage carePackage;
    LoginPackage loginPackage;
    ABC_CHECK(carePackage.load(login.paths.carePackagePath()));
    ABC_CHECK(loginPackage.load(login.paths.loginPackagePath()));

    // Load the old keys:
    DataChunk authKey = login.authKey();

    // Update scrypt parameters:
    JsonSnrp snrp;
    ABC_CHECK(snrp.create());
    ABC_CHECK(carePackage.snrp3Set(snrp));
    ABC_CHECK(snrp.create());
    ABC_CHECK(carePackage.snrp4Set(snrp));

    // Make questionKey (unlocks questions):
    DataChunk questionKey;
    ABC_CHECK(carePackage.snrp4().hash(questionKey, login.lobby.username()));

    // Encrypt the questions:
    JsonBox box;
    ABC_CHECK(box.encrypt(recoveryQuestions, questionKey));
    ABC_CHECK(carePackage.questionBoxSet(box));

    // Make recoveryKey (unlocks dataKey):
    DataChunk recoveryKey;
    ABC_CHECK(carePackage.snrp3().hash(recoveryKey, LRA));

    // Encrypt dataKey:
    ABC_CHECK(box.encrypt(login.dataKey(), recoveryKey));
    ABC_CHECK(loginPackage.recoveryBoxSet(box));

    // Make recoveryAuthKey (unlocks the server):
    DataChunk recoveryAuthKey;
    ABC_CHECK(usernameSnrp().hash(recoveryAuthKey, LRA));

    // Encrypt recoveryAuthKey (needed for atomic password updates):
    ABC_CHECK(box.encrypt(recoveryAuthKey, login.dataKey()));
    ABC_CHECK(loginPackage.ELRA1Set(box));

    // Change the server login:
    ABC_CHECK(loginServerChangePassword(login, authKey, recoveryAuthKey,
                                        carePackage, loginPackage));

    // Change the on-disk login:
    ABC_CHECK(carePackage.save(login.paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(login.paths.loginPackagePath()));

    return Status();
}

} // namespace abcd
