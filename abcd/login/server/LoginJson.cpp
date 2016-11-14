/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginJson.hpp"
#include "../LoginPackages.hpp"
#include "../LoginPin2.hpp"
#include "../LoginRecovery2.hpp"
#include "../../AccountPaths.hpp"

namespace abcd {

Status
LoginJson::save(const AccountPaths &paths, DataSlice dataKey)
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
    if (repos().ok())
        ABC_CHECK(repos().save(paths.reposPath()));

    // Keys to save unencrypted:
    DataChunk recovery2Key;
    if (recovery2KeyBox().decrypt(recovery2Key, dataKey))
        ABC_CHECK(loginRecovery2KeySave(recovery2Key, paths));

    DataChunk pin2Key;
    if (pin2KeyBox().decrypt(pin2Key, dataKey))
        ABC_CHECK(loginPin2KeySave(pin2Key, paths));

    // Write to disk:
    ABC_CHECK(carePackage.save(paths.carePackagePath()));
    ABC_CHECK(loginPackage.save(paths.loginPackagePath()));

    return Status();
}

} // namespace abcd
