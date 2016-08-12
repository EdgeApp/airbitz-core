/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Helper functions for dealing with login and care packages.
 */

#ifndef ABCD_LOGIN_LOGIN_PACKAGES_HPP
#define ABCD_LOGIN_LOGIN_PACKAGES_HPP

#include "../json/JsonSnrp.hpp"
#include "../json/JsonBox.hpp"

namespace abcd {

/**
 * A round-trippable representation of the AirBitz CarePackage file.
 */
struct CarePackage:
    public JsonObject
{
    ABC_JSON_VALUE(passwordKeySnrp, "SNRP2", JsonSnrp)
    ABC_JSON_VALUE(recoveryKeySnrp, "SNRP3", JsonSnrp)
    ABC_JSON_VALUE(questionKeySnrp, "SNRP4", JsonSnrp)
    ABC_JSON_VALUE(questionBox, "ERQ", JsonBox) // Optional
};

/**
 * A round-trippable representation of the AirBitz LoginPackage file.
 */
struct LoginPackage:
    public JsonObject
{
    ABC_JSON_VALUE(passwordBox, "EMK_LP2",  JsonBox)
    ABC_JSON_VALUE(recoveryBox, "EMK_LRA3", JsonBox) // Optional

    // These are all encrypted with MK:
    ABC_JSON_VALUE(syncKeyBox,  "ESyncKey", JsonBox)
    ABC_JSON_VALUE(passwordAuthBox, "ELP1", JsonBox)
    ABC_JSON_VALUE(ELRA1,       "ELRA1",    JsonBox) // Optional
    /* There was a time when the login and password were not orthogonal.
     * Therefore, any updates to one needed to include the other for
     * atomic consistency. The login refactor solved this problem, but
     * the server API still uses the old update-the-world technique.
     * The ELRA1 can go away once the server API allows for independent
     * login and password changes.
     *
     * The ELP1 is useful by itself for things like uploading error logs.
     * If we ever associate public keys with logins (like for wallet
     * sharing), those can replace the ELP1.
     *
     * Since LP1 is always available, there is never a time where
     * changing the password or recovery would need to pass the old
     * recovery answers. The client-side routines no longer take an
     * oldLRA1 parameter, but the server API still does.
     */
};

} // namespace abcd

#endif
