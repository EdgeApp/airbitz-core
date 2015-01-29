/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginPIN.hpp"
#include "Login.hpp"
#include "LoginDir.hpp"
#include "LoginServer.hpp"
#include "util/Json.hpp"
#include "util/Util.hpp"
#include <jansson.h>

namespace abcd {

#define KEY_LENGTH 32

#define PIN_FILENAME                            "PinPackage.json"
#define JSON_LOCAL_EMK_PINK_FIELD               "EMK_PINK"
#define JSON_LOCAL_DID_FIELD                    "DID"
#define JSON_LOCAL_EXPIRES_FIELD                "Expires"

/**
 * A round-trippable representation of the PIN-based re-login file.
 */
typedef struct sABC_PinLocal
{
    json_t          *pEMK_PINK;
    tABC_U08Buf     DID;
    time_t          expires;
} tABC_PinLocal;

/**
 * Frees the PIN package.
 */
static
void ABC_LoginPinLocalFree(tABC_PinLocal *pSelf)
{
    if (pSelf)
    {
        if (pSelf->pEMK_PINK) json_decref(pSelf->pEMK_PINK);
        ABC_BUF_FREE(pSelf->DID);

        ABC_CLEAR_FREE(pSelf, sizeof(tABC_PinLocal));
    }
}

/**
 * Loads the PIN package from disk.
 */
static
tABC_CC ABC_LoginPinLocalLoad(tABC_PinLocal **ppSelf,
                              unsigned AccountNum,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t je;
    int e;

    tABC_PinLocal       *pSelf          = NULL;
    char *              szLocal         = NULL;
    json_t              *pLocal         = NULL;
    char *              szDID           = NULL;
    json_int_t          expires         = 0;

    ABC_NEW(pSelf, tABC_PinLocal);

    // Load the local file:
    ABC_CHECK_RET(ABC_LoginDirFileLoad(&szLocal, AccountNum, PIN_FILENAME, pError));
    pLocal = json_loads(szLocal, 0, &je);
    ABC_CHECK_ASSERT(pLocal != NULL && json_is_object(pLocal),
        ABC_CC_JSONError, "Error parsing local PIN JSON");
    e = json_unpack(pLocal, "{s:O, s:s, s:I}",
        JSON_LOCAL_EMK_PINK_FIELD, &pSelf->pEMK_PINK,
        JSON_LOCAL_DID_FIELD, &szDID,
        JSON_LOCAL_EXPIRES_FIELD, &expires);
    ABC_CHECK_SYS(!e, "Error parsing local PIN JSON");

    // Unpack items:
    ABC_CHECK_RET(ABC_CryptoBase64Decode(szDID, &pSelf->DID, pError));
    pSelf->expires = expires;

    // Assign the final output:
    *ppSelf = pSelf;
    pSelf = NULL;

exit:
    ABC_LoginPinLocalFree(pSelf);
    ABC_FREE_STR(szLocal);
    if (pLocal)         json_decref(pLocal);
    // ABC_FREE_STR(szDID); // Freeing pLocal frees this too

    return cc;
}

/**
 * Determines whether or not the given user can log in via PIN on this
 * device.
 */
tABC_CC ABC_LoginPinExists(const char *szUserName,
                           bool *pbExists,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Error error;

    tABC_PinLocal *pLocal = NULL;
    char *szFixed = NULL;
    int AccountNum;

    ABC_CHECK_RET(ABC_LoginFixUserName(szUserName, &szFixed, pError));
    ABC_CHECK_RET(ABC_LoginDirGetNumber(szFixed, &AccountNum, pError));

    *pbExists = false;
    if (ABC_CC_Ok == ABC_LoginPinLocalLoad(&pLocal, AccountNum, &error))
    {
        *pbExists = true;
    }

exit:
    ABC_LoginPinLocalFree(pLocal);
    ABC_FREE_STR(szFixed);

    return cc;
}

/**
 * Deletes the local copy of the PIN-based login data.
 */
tABC_CC ABC_LoginPinDelete(const char *szUserName,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFixed = NULL;
    int AccountNum;

    ABC_CHECK_RET(ABC_LoginFixUserName(szUserName, &szFixed, pError));
    ABC_CHECK_RET(ABC_LoginDirGetNumber(szFixed, &AccountNum, pError));
    ABC_CHECK_RET(ABC_LoginDirFileDelete(AccountNum, PIN_FILENAME, pError));

exit:
    ABC_FREE_STR(szFixed);

    return cc;
}

/**
 * Assuming a PIN-based login pagage exits, log the user in.
 */
tABC_CC ABC_LoginPin(tABC_Login **ppSelf,
                     const char *szUserName,
                     const char *szPIN,
                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t je;

    tABC_Login          *pSelf          = NULL;
    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    tABC_PinLocal       *pLocal         = NULL;
    json_t              *pEPINK         = NULL;
    char *              szEPINK         = NULL;
    AutoU08Buf          LPIN;
    AutoU08Buf          LPIN1;
    AutoU08Buf          LPIN2;
    AutoU08Buf          PINK;

    // Allocate self:
    ABC_CHECK_RET(ABC_LoginNew(&pSelf, szUserName, pError));

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(pSelf->AccountNum, &pCarePackage, &pLoginPackage, pError));
    ABC_CHECK_RET(ABC_LoginPinLocalLoad(&pLocal, pSelf->AccountNum, pError));

    // LPIN = L + PIN:
    ABC_BUF_STRCAT(LPIN, pSelf->szUserName, szPIN);
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LPIN, pCarePackage->pSNRP1, &LPIN1, pError));
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LPIN, pCarePackage->pSNRP2, &LPIN2, pError));

    // Get EPINK from the server:
    ABC_CHECK_RET(ABC_LoginServerGetPinPackage(pLocal->DID, LPIN1, &szEPINK, pError));
    pEPINK = json_loads(szEPINK, 0, &je);
    ABC_CHECK_ASSERT(pEPINK != NULL && json_is_object(pEPINK),
        ABC_CC_JSONError, "Error parsing EPINK JSON");

    // Decrypt MK:
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pEPINK, LPIN2, &PINK, pError));
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pLocal->pEMK_PINK, PINK, &pSelf->MK, pError));

    // Decrypt SyncKey:
    ABC_CHECK_RET(ABC_LoginPackageGetSyncKey(pLoginPackage, pSelf->MK, &pSelf->szSyncKey, pError));

    // Assign the final output:
    *ppSelf = pSelf;
    pSelf = NULL;

exit:
    ABC_LoginFree(pSelf);
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);
    ABC_LoginPinLocalFree(pLocal);
    if (pEPINK)         json_decref(pEPINK);
    ABC_FREE_STR(szEPINK);

    if (ABC_CC_PinExpired == cc)
    {
        tABC_Error error;
        ABC_LoginPinDelete(szUserName, &error);
    }

    return cc;
}

/**
 * Sets up a PIN login package, both on-disk and on the server.
 */
tABC_CC ABC_LoginPinSetup(tABC_Login *pSelf,
                          const char *szPIN,
                          time_t expires,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    json_t              *pEMK_PINK      = NULL;
    json_t              *pEPINK         = NULL;
    json_t              *pLocal         = NULL;
    char *              szDID           = NULL;
    char *              szEPINK         = NULL;
    char *              szLocal         = NULL;
    AutoU08Buf          L1;
    AutoU08Buf          LP1;
    AutoU08Buf          LPIN;
    AutoU08Buf          LPIN1;
    AutoU08Buf          LPIN2;
    AutoU08Buf          PINK;
    AutoU08Buf          DID;

    // Get login stuff:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(pSelf->AccountNum, &pCarePackage, &pLoginPackage, pError));
    ABC_CHECK_RET(ABC_LoginGetServerKeys(pSelf, &L1, &LP1, pError));

    // LPIN = L + PIN:
    ABC_BUF_STRCAT(LPIN, pSelf->szUserName, szPIN);
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LPIN, pCarePackage->pSNRP1, &LPIN1, pError));
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LPIN, pCarePackage->pSNRP2, &LPIN2, pError));

    // Set up PINK stuff:
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(KEY_LENGTH, &PINK, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->MK, PINK,
        ABC_CryptoType_AES256, &pEMK_PINK, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(PINK, LPIN2,
        ABC_CryptoType_AES256, &pEPINK, pError));
    szEPINK = ABC_UtilStringFromJSONObject(pEPINK, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    ABC_CHECK_NULL(szEPINK);

    // Set up DID:
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(KEY_LENGTH, &DID, pError));
    ABC_CHECK_RET(ABC_CryptoBase64Encode(DID, &szDID, pError));

    // Set up the local file:
    pLocal = json_pack("{s:O, s:s, s:I}",
        JSON_LOCAL_EMK_PINK_FIELD, pEMK_PINK,
        JSON_LOCAL_DID_FIELD, szDID,
        JSON_LOCAL_EXPIRES_FIELD, (json_int_t)expires);
    ABC_CHECK_NULL(pLocal);
    szLocal = ABC_UtilStringFromJSONObject(pLocal, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    ABC_CHECK_NULL(szLocal);
    ABC_CHECK_RET(ABC_LoginDirFileSave(szLocal, pSelf->AccountNum, PIN_FILENAME, pError));

    // Set up the server:
    ABC_CHECK_RET(ABC_LoginServerUpdatePinPackage(L1, LP1, DID, LPIN1, szEPINK, expires, pError));

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);
    if (pEMK_PINK)      json_decref(pEMK_PINK);
    if (pEPINK)         json_decref(pEPINK);
    if (pLocal)         json_decref(pLocal);
    ABC_FREE_STR(szDID);
    ABC_FREE_STR(szEPINK);
    ABC_FREE_STR(szLocal);

    return cc;
}

} // namespace abcd
