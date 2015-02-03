/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Login.hpp"
#include "LoginDir.hpp"
#include "LoginServer.hpp"
#include "../Account.hpp"
#include "../util/Crypto.hpp"
#include "../util/Util.hpp"
#include <ctype.h>

namespace abcd {

#define ACCOUNT_MK_LENGTH 32

Login::~Login()
{
    ABC_FREE_STR(szUserName);
    ABC_BUF_FREE(L1);

    ABC_BUF_FREE(MK);
    ABC_FREE_STR(szSyncKey);
}

Login::Login()
{
    szUserName = nullptr;
    ABC_BUF_CLEAR(L1);
    ABC_BUF_CLEAR(MK);
    szSyncKey = nullptr;
}

/**
 * Deletes a login object and all its contents.
 */
void ABC_LoginFree(tABC_Login *pSelf)
{
    if (pSelf)
        delete pSelf;
}

/**
 * Sets up the username and L1 parameters in the login object.
 */
tABC_CC ABC_LoginNew(tABC_Login **ppSelf,
                     const char *szUserName,
                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf L = ABC_BUF_NULL; // Do not free
    AutoFree<tABC_CryptoSNRP, ABC_CryptoFreeSNRP> pSNRP0;
    Login *pSelf = new Login;

    // Set up identity:
    ABC_CHECK_RET(ABC_LoginFixUserName(szUserName, &pSelf->szUserName, pError));
    ABC_CHECK_RET(ABC_LoginDirGetName(pSelf->szUserName, pSelf->directory, pError));

    // Create L1:
    ABC_BUF_SET_PTR(L, (unsigned char *)pSelf->szUserName, strlen(pSelf->szUserName));
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForServer(&pSNRP0.get(), pError));
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(L, pSNRP0, &pSelf->L1, pError));

    *ppSelf = pSelf;
    pSelf = NULL;

exit:
    if (pSelf)          ABC_LoginFree(pSelf);

    return cc;
}

/**
 * Creates a new login account, both on-disk and on the server.
 *
 * @param szUserName    The user name for the account.
 * @param szPassword    The password for the account.
 * @param ppSelf        The returned login object.
 */
tABC_CC ABC_LoginCreate(const char *szUserName,
                        const char *szPassword,
                        tABC_Login **ppSelf,
                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_Login          *pSelf          = NULL;
    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;
    AutoU08Buf           SyncKey;
    AutoU08Buf           LP;
    AutoU08Buf           LP1;
    AutoU08Buf           LP2;

    // Allocate self:
    ABC_CHECK_RET(ABC_LoginNew(&pSelf, szUserName, pError));

    // Set up packages:
    ABC_CHECK_RET(ABC_CarePackageNew(&pCarePackage, pError));
    ABC_NEW(pLoginPackage, tABC_LoginPackage);

    // Generate MK:
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(ACCOUNT_MK_LENGTH, &pSelf->MK, pError));

    // Generate SyncKey:
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(SYNC_KEY_LENGTH, &SyncKey, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(SyncKey, &pSelf->szSyncKey, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, pSelf->szUserName, szPassword);

    // Set up EMK_LP2:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->MK, LP2,
        ABC_CryptoType_AES256, &pLoginPackage->EMK_LP2, pError));

    // Set up ESyncKey:
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(SyncKey, pSelf->MK,
        ABC_CryptoType_AES256, &pLoginPackage->ESyncKey, pError));

    // Set up ELP1:
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pCarePackage->pSNRP1, &LP1, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(LP1, pSelf->MK,
        ABC_CryptoType_AES256, &pLoginPackage->ELP1, pError));

    // Create the account and repo on server:
    ABC_CHECK_RET(ABC_LoginServerCreate(pSelf->L1, LP1,
        pCarePackage, pLoginPackage, pSelf->szSyncKey, pError));

    // Latch the account:
    ABC_CHECK_RET(ABC_LoginServerActivate(pSelf->L1, LP1, pError));

    // Set up the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirCreate(pSelf->directory, pSelf->szUserName, pError));
    ABC_CHECK_RET(ABC_LoginDirSavePackages(pSelf->directory, pCarePackage, pLoginPackage, pError));

    // Assign the final output:
    *ppSelf = pSelf;
    pSelf = NULL;

exit:
    ABC_LoginFree(pSelf);
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);
    return cc;
}

/**
 * Determines whether or not the given string matches the account's
 * username.
 * @param szUserName    The user name to check.
 * @param pMatch        Set to 1 if the names match.
 */
tABC_CC ABC_LoginCheckUserName(tABC_Login *pSelf,
                               const char *szUserName,
                               int *pMatch,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFixed = NULL;
    *pMatch = 0;

    ABC_CHECK_RET(ABC_LoginFixUserName(szUserName, &szFixed, pError));
    if (!strcmp(szFixed, pSelf->szUserName))
        *pMatch = 1;

exit:
    ABC_FREE_STR(szFixed);
    return cc;
}

/**
 * Obtains the sync keys for accessing an account's repo.
 * @param ppKeys    The returned keys. Call ABC_SyncFreeKeys when done.
 */
tABC_CC ABC_LoginGetSyncKeys(tABC_Login *pSelf,
                             tABC_SyncKeys **ppKeys,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoSyncKeys pKeys;

    ABC_NEW(pKeys.get(), tABC_SyncKeys);
    ABC_CHECK_RET(ABC_LoginDirGetSyncDir(pSelf->directory, &pKeys->szSyncDir, pError));
    ABC_BUF_DUP(pKeys->MK, pSelf->MK);
    ABC_STRDUP(pKeys->szSyncKey, pSelf->szSyncKey);

    *ppKeys = pKeys;
    pKeys.get() = NULL;

exit:
    return cc;
}

/**
 * Obtains an account object's user name.
 * @param pL1       The hashed user name. The caller must free this.
 * @param pLP1      The hashed user name & password. The caller must free this.
 */
tABC_CC ABC_LoginGetServerKeys(tABC_Login *pSelf,
                               tABC_U08Buf *pL1,
                               tABC_U08Buf *pLP1,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_CarePackage *pCarePackage = NULL;
    tABC_LoginPackage *pLoginPackage = NULL;

    ABC_BUF_DUP(*pL1, pSelf->L1);

    ABC_CHECK_RET(ABC_LoginDirLoadPackages(pSelf->directory, &pCarePackage, &pLoginPackage, pError));
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pLoginPackage->ELP1, pSelf->MK, pLP1, pError));

exit:
    ABC_CarePackageFree(pCarePackage);
    ABC_LoginPackageFree(pLoginPackage);

    return cc;
}

/**
 * Re-formats a username to all-lowercase, checking for disallowed
 * characters and collapsing spaces.
 */
tABC_CC ABC_LoginFixUserName(const char *szUserName,
                             char **pszOut,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szOut = NULL;
    const char *si;
    char *di;

    ABC_STR_NEW(szOut, strlen(szUserName) + 1);

    // Collapse leading & internal spaces:
    si = szUserName;
    di = szOut;
    do
    {
        while (isspace(*si))
            ++si;
        while (*si && !isspace(*si))
            *di++ = *si++;
        *di++ = ' ';
    }
    while (*si);
    *--di = 0;

    // Stomp trailing space, if any:
    --di;
    if (szOut < di && ' ' == *di)
        *di = 0;

    // Scan for bad characters, and make lowercase:
    for (di = szOut; *di; ++di)
    {
        if (*di < ' ' || '~' < *di)
            cc = ABC_CC_NotSupported;
        if ('A' <= *di && *di <= 'Z')
            *di = *di - 'A' + 'a';
    }

    *pszOut = szOut;
    szOut = NULL;

exit:
    ABC_FREE_STR(szOut);
    return cc;
}

} // namespace abcd
