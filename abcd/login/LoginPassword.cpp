/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginPassword.hpp"
#include "LoginDir.hpp"
#include "LoginServer.hpp"
#include "../util/Util.hpp"

namespace abcd {

static
tABC_CC ABC_LoginPasswordDisk(tABC_Login *pSelf,
                              tABC_U08Buf LP,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    AutoU08Buf          LP2;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(pSelf->AccountNum, &pCarePackage, &pLoginPackage, pError));

    // Decrypt MK:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pLoginPackage->EMK_LP2, LP2, &pSelf->MK, pError));

    // Decrypt SyncKey:
    ABC_CHECK_RET(ABC_LoginPackageGetSyncKey(pLoginPackage, pSelf->MK, &pSelf->szSyncKey, pError));

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

static
tABC_CC ABC_LoginPasswordServer(tABC_Login *pSelf,
                                tABC_U08Buf LP,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    tABC_U08Buf         LRA1            = ABC_BUF_NULL; // Do not free
    AutoU08Buf          LP1;
    AutoU08Buf          LP2;

    // Get the CarePackage:
    ABC_CHECK_RET(ABC_LoginServerGetCarePackage(pSelf->L1, &pCarePackage, pError));

    // Get the LoginPackage:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP1, &LP1, pError));
    ABC_CHECK_RET(ABC_LoginServerGetLoginPackage(pSelf->L1, LP1, LRA1, &pLoginPackage, pError));

    // Decrypt MK:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pLoginPackage->EMK_LP2, LP2, &pSelf->MK, pError));

    // Decrypt SyncKey:
    ABC_CHECK_RET(ABC_LoginPackageGetSyncKey(pLoginPackage, pSelf->MK, &pSelf->szSyncKey, pError));

    // Set up the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirCreate(&pSelf->AccountNum, pSelf->szUserName, pError));
    ABC_CHECK_RET(ABC_LoginDirSavePackages(pSelf->AccountNum, pCarePackage, pLoginPackage, pError));

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

/**
 * Loads an existing login object, either from the server or from disk.
 *
 * @param szUserName    The user name for the account.
 * @param szPassword    The password for the account.
 * @param ppSelf        The returned login object.
 */
tABC_CC ABC_LoginPassword(tABC_Login **ppSelf,
                          const char *szUserName,
                          const char *szPassword,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Error error;

    tABC_Login          *pSelf          = NULL;
    AutoU08Buf          LP;

    // Allocate self:
    ABC_CHECK_RET(ABC_LoginNew(&pSelf, szUserName, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, pSelf->szUserName, szPassword);

    // Try the login both ways:
    cc = ABC_LoginPasswordDisk(pSelf, LP, &error);
    if (ABC_CC_Ok != cc)
    {
        ABC_CHECK_RET(ABC_LoginPasswordServer(pSelf, LP, pError));
    }

    // Assign the final output:
    *ppSelf = pSelf;
    pSelf = NULL;

exit:
    ABC_LoginFree(pSelf);

    return cc;
}

/**
 * Changes the password on an existing login object.
 * @param pSelf         An already-loaded login object.
 * @param szPassword    The new password.
 */
tABC_CC ABC_LoginPasswordSet(tABC_Login *pSelf,
                             const char *szPassword,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_CarePackage *pCarePackage = NULL;
    tABC_LoginPackage *pLoginPackage = NULL;
    AutoU08Buf oldL1;
    AutoU08Buf oldLP1;
    AutoU08Buf oldLRA1;
    AutoU08Buf LP;
    AutoU08Buf LP1;
    AutoU08Buf LP2;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(pSelf->AccountNum, &pCarePackage, &pLoginPackage, pError));

    // Load the old keys:
    ABC_CHECK_RET(ABC_LoginGetServerKeys(pSelf, &oldL1, &oldLP1, pError));
    if (pLoginPackage->ELRA1)
    {
        ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pLoginPackage->ELRA1, pSelf->MK, &oldLRA1, pError));
    }

    // Update SNRP2:
    ABC_CryptoFreeSNRP(pCarePackage->pSNRP2);
    pCarePackage->pSNRP2 = nullptr;
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&pCarePackage->pSNRP2, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, pSelf->szUserName, szPassword);

    // Update EMK_LP2:
    json_decref(pLoginPackage->EMK_LP2);
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->MK, LP2,
        ABC_CryptoType_AES256, &pLoginPackage->EMK_LP2, pError));

    // Update ELP1:
    json_decref(pLoginPackage->ELP1);
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP1, &LP1, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(LP1, pSelf->MK,
        ABC_CryptoType_AES256, &pLoginPackage->ELP1, pError));

    // Change the server login:
    ABC_CHECK_RET(ABC_LoginServerChangePassword(oldL1, oldLP1,
        LP1, oldLRA1, pCarePackage, pLoginPackage, pError));

    // Change the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirSavePackages(pSelf->AccountNum, pCarePackage, pLoginPackage, pError));

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

/**
 * Validates that the provided password is correct.
 * This is used in the GUI to guard access to certain actions.
 *
 * @param pSelf         An already-loaded login object.
 * @param szPassword    The password to check.
 * @param pOk           Set to true if the password is good, or false otherwise.
 */
tABC_CC ABC_LoginPasswordOk(tABC_Login *pSelf,
                            const char *szPassword,
                            bool *pOk,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_CarePackage *pCarePackage = NULL;
    tABC_LoginPackage *pLoginPackage = NULL;
    AutoU08Buf LP;
    AutoU08Buf LP2;
    AutoU08Buf MK;

    *pOk = false;

    // Load the packages:
    ABC_CHECK_RET(ABC_LoginDirLoadPackages(pSelf->AccountNum, &pCarePackage, &pLoginPackage, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, pSelf->szUserName, szPassword);
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));

    // Try to decrypt MK:
    tABC_Error error;
    if (ABC_CC_Ok == ABC_CryptoDecryptJSONObject(pLoginPackage->EMK_LP2, LP2, &MK, &error))
    {
        *pOk = true;
    }

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

} // namespace abcd
