/**
 * @file
 * Helper functions for dealing with login and care packages.
 */

#ifndef ABC_LoginPackage_h
#define ABC_LoginPackage_h

#include "ABC.h"
#include "util/ABC_Crypto.h"
#include <jansson.h>

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * A round-trippable representation of the AirBitz CarePackage file.
     */
    typedef struct sABC_CarePackage
    {
        tABC_CryptoSNRP *pSNRP2;
        tABC_CryptoSNRP *pSNRP3;
        tABC_CryptoSNRP *pSNRP4;
        json_t          *ERQ;       // Optional
    } tABC_CarePackage;

    void ABC_CarePackageFree(tABC_CarePackage *pSelf);

    tABC_CC ABC_CarePackageDecode(tABC_CarePackage **ppSelf,
                                  char *szCarePackage,
                                  tABC_Error *pError);

    tABC_CC ABC_CarePackageEncode(tABC_CarePackage *pSelf,
                                  char **pszCarePackage,
                                  tABC_Error *pError);

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
         */
    } tABC_LoginPackage;

    void ABC_LoginPackageFree(tABC_LoginPackage *pSelf);

    tABC_CC ABC_LoginPackageDecode(tABC_LoginPackage **ppSelf,
                                   char *szLoginPackage,
                                   tABC_Error *pError);

    tABC_CC ABC_LoginPackageEncode(tABC_LoginPackage *pSelf,
                                   char **pszLoginPackage,
                                   tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
