/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginRecovery.hpp"
#include "Login.hpp"
#include "LoginStore.hpp"
#include "json/AuthJson.hpp"
#include "json/LoginJson.hpp"
#include "json/LoginPackages.hpp"
#include "server/LoginServer.hpp"
#include "../json/JsonBox.hpp"

namespace abcd {

Status
loginRecoveryQuestions(std::string &result, LoginStore &store)
{
    // Grab the login information from the server:
    AuthJson authJson;
    LoginReplyJson loginJson;
    ABC_CHECK(authJson.userIdSet(store));
    ABC_CHECK(loginServerLogin(loginJson, authJson));

    // Verify that the questions exist:
    if (!loginJson.questionBox())
        return ABC_ERROR(ABC_CC_NoRecoveryQuestions, "No recovery questions");

    // Decrypt:
    DataChunk questionKey;
    DataChunk questions;
    ABC_CHECK(loginJson.questionKeySnrp().hash(questionKey, store.username()));
    ABC_CHECK(loginJson.questionBox().decrypt(questions, questionKey));

    result = toString(questions);
    return Status();
}

Status
loginRecovery(std::shared_ptr<Login> &result,
              LoginStore &store, const std::string &recoveryAnswers,
              AuthError &authError)
{
    const auto LRA = store.username() + recoveryAnswers;

    // Create recoveryAuth:
    DataChunk recoveryAuth;
    ABC_CHECK(usernameSnrp().hash(recoveryAuth, LRA));

    // Grab the login information from the server:
    AuthJson authJson;
    LoginReplyJson loginJson;
    ABC_CHECK(authJson.recoverySet(store, recoveryAuth));
    ABC_CHECK(loginServerLogin(loginJson, authJson, &authError));

    // Unlock recoveryBox:
    DataChunk recoveryKey;
    DataChunk dataKey;
    ABC_CHECK(loginJson.recoveryKeySnrp().hash(recoveryKey, LRA));
    ABC_CHECK(loginJson.recoveryBox().decrypt(dataKey, recoveryKey));

    // Create the Login object:
    ABC_CHECK(Login::createOnline(result, store, dataKey, loginJson));
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

    // Change the server login:
    ABC_CHECK(loginServerChangePassword(login, passwordAuth, recoveryAuth,
                                        carePackage, loginPackage));

    // Change the on-disk login:
    ABC_CHECK(carePackage.save(login.paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(login.paths.loginPackagePath()));

    return Status();
}

} // namespace abcd
