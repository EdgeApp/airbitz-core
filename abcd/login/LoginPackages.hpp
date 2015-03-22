/*
 * Copyright (c) 2014, AirBitz, Inc.
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

#include "../crypto/Scrypt.hpp"
#include "../json/JsonBox.hpp"

namespace abcd {

/**
 * A round-trippable representation of the AirBitz CarePackage file.
 */
struct CarePackage:
    public JsonObject
{
    ABC_JSON_VALUE(snrp2,       "SNRP2",    JsonSnrp)
    ABC_JSON_VALUE(snrp3,       "SNRP3",    JsonSnrp)
    ABC_JSON_VALUE(snrp4,       "SNRP4",    JsonSnrp)
    ABC_JSON_VALUE(questionBox, "ERQ",      JsonBox) // Optional
};

/**
 * A round-trippable representation of the AirBitz LoginPackage file.
 */
typedef struct sABC_LoginPackage
{
    json_t          *EMK_LP2;
    json_t          *EMK_LRA3;  // Optional
    // These are all encrypted with MK:
    json_t          *ESyncKey;
    json_t          *ELP1;
    json_t          *ELRA1;     // Optional
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
} tABC_LoginPackage;

void ABC_LoginPackageFree(tABC_LoginPackage *pSelf);

tABC_CC ABC_LoginPackageDecode(tABC_LoginPackage **ppSelf,
                               char *szLoginPackage,
                               tABC_Error *pError);

tABC_CC ABC_LoginPackageEncode(tABC_LoginPackage *pSelf,
                               char **pszLoginPackage,
                               tABC_Error *pError);

tABC_CC ABC_LoginPackageGetSyncKey(tABC_LoginPackage *pSelf,
                                   const tABC_U08Buf MK,
                                   char **pszSyncKey,
                                   tABC_Error *pError);

} // namespace abcd

#endif
