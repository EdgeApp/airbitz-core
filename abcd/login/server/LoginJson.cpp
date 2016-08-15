/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginJson.hpp"
#include "../LoginPackages.hpp"
#include "../../AccountPaths.hpp"

namespace abcd {

Status
LoginJson::save(const AccountPaths &paths)
{
    CarePackage carePackage;
    LoginPackage loginPackage;

    // Password login:
    if (passwordBox().ok())
        ABC_CHECK(loginPackage.passwordBoxSet(passwordBox()));
    if (passwordKeySnrp().ok())
        ABC_CHECK(carePackage.passwordKeySnrpSet(passwordKeySnrp()));

    // Recovery:
    if (questionBox().ok())
        ABC_CHECK(carePackage.questionBoxSet(questionBox()));
    if (questionKeySnrp().ok())
        ABC_CHECK(carePackage.questionKeySnrpSet(questionKeySnrp()));
    if (recoveryBox().ok())
        ABC_CHECK(loginPackage.recoveryBoxSet(recoveryBox()));
    if (recoveryKeySnrp().ok())
        ABC_CHECK(carePackage.recoveryKeySnrpSet(recoveryKeySnrp()));

    // Keys:
    if (passwordAuthBox().ok())
        ABC_CHECK(loginPackage.passwordAuthBoxSet(passwordAuthBox()));
    if (rootKeyBox().ok())
        ABC_CHECK(rootKeyBox().save(paths.rootKeyPath()));
    if (syncKeyBox().ok())
        ABC_CHECK(loginPackage.syncKeyBoxSet(syncKeyBox()));

    ABC_CHECK(carePackage.save(paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(paths.loginPackagePath()));

    return Status();
}

} // namespace abcd
