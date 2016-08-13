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
};

} // namespace abcd

#endif
