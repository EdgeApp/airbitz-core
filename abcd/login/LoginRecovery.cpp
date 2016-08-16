/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginRecovery.hpp"
#include "Login.hpp"
#include "LoginPackages.hpp"
#include "LoginStore.hpp"
#include "server/LoginServer.hpp"
#include "../json/JsonBox.hpp"

namespace abcd {

Status
loginRecoveryQuestions(std::string &result, LoginStore &store)
{
    // Load CarePackage:
    CarePackage carePackage;
    ABC_CHECK(loginServerGetCarePackage(store, carePackage));

    // Verify that the questions exist:
    if (!carePackage.questionBox())
        return ABC_ERROR(ABC_CC_NoRecoveryQuestions, "No recovery questions");

    // Create questionKey (unlocks questions):
    DataChunk questionKey;
    ABC_CHECK(carePackage.questionKeySnrp().hash(questionKey, store.username()));

    // Decrypt:
    DataChunk questions;
    ABC_CHECK(carePackage.questionBox().decrypt(questions, questionKey));

    result = toString(questions);
    return Status();
}

Status
loginRecovery(std::shared_ptr<Login> &result,
              LoginStore &store, const std::string &recoveryAnswers,
              AuthError &authError)
{
    std::string LRA = store.username() + recoveryAnswers;

    // Get the CarePackage:
    CarePackage carePackage;
    ABC_CHECK(loginServerGetCarePackage(store, carePackage));

    // Make recoveryAuth (unlocks the server):
    DataChunk recoveryAuth;
    ABC_CHECK(usernameSnrp().hash(recoveryAuth, LRA));

    // Get the LoginPackage:
    LoginPackage loginPackage;
    JsonPtr rootKeyBox;
    ABC_CHECK(loginServerGetLoginPackage(store, DataSlice(), recoveryAuth,
                                         loginPackage, rootKeyBox,
                                         authError));

    // Make recoveryKey (unlocks dataKey):
    DataChunk recoveryKey;
    ABC_CHECK(carePackage.recoveryKeySnrp().hash(recoveryKey, LRA));

    // Decrypt dataKey (unlocks the account):
    DataChunk dataKey;
    ABC_CHECK(loginPackage.recoveryBox().decrypt(dataKey, recoveryKey));

    // Create the Login object:
    std::shared_ptr<Login> out;
    ABC_CHECK(Login::createOnline(out, store, dataKey,
                                  loginPackage, rootKeyBox));

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
    std::string LRA = login.store.username() + recoveryAnswers;

    // Load the packages:
    CarePackage carePackage;
    LoginPackage loginPackage;
    ABC_CHECK(carePackage.load(login.paths.carePackagePath()));
    ABC_CHECK(loginPackage.load(login.paths.loginPackagePath()));

    // Load the old keys:
    DataChunk passwordAuth = login.passwordAuth();

    // Update scrypt parameters:
    JsonSnrp snrp;
    ABC_CHECK(snrp.create());
    ABC_CHECK(carePackage.recoveryKeySnrpSet(snrp));
    ABC_CHECK(snrp.create());
    ABC_CHECK(carePackage.questionKeySnrpSet(snrp));

    // Make questionKey (unlocks questions):
    DataChunk questionKey;
    ABC_CHECK(carePackage.questionKeySnrp().hash(questionKey,
              login.store.username()));

    // Encrypt the questions:
    JsonBox questionBox;
    ABC_CHECK(questionBox.encrypt(recoveryQuestions, questionKey));
    ABC_CHECK(carePackage.questionBoxSet(questionBox));

    // Make recoveryKey (unlocks dataKey):
    DataChunk recoveryKey;
    ABC_CHECK(carePackage.recoveryKeySnrp().hash(recoveryKey, LRA));

    // Encrypt dataKey:
    JsonBox recoveryBox;
    ABC_CHECK(recoveryBox.encrypt(login.dataKey(), recoveryKey));
    ABC_CHECK(loginPackage.recoveryBoxSet(recoveryBox));

    // Make recoveryAuth (unlocks the server):
    DataChunk recoveryAuth;
    ABC_CHECK(usernameSnrp().hash(recoveryAuth, LRA));

    // Encrypt recoveryAuth (needed for atomic password updates):
    JsonBox recoveryAuthBox;
    ABC_CHECK(recoveryAuthBox.encrypt(recoveryAuth, login.dataKey()));
    ABC_CHECK(loginPackage.ELRA1Set(recoveryAuthBox));

    // Change the server login:
    ABC_CHECK(loginServerChangePassword(login, passwordAuth, recoveryAuth,
                                        carePackage, loginPackage));

    // Change the on-disk login:
    ABC_CHECK(carePackage.save(login.paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(login.paths.loginPackagePath()));

    return Status();
}

} // namespace abcd
