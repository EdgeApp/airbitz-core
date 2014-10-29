/**
 * @file
 * An object representing a logged-in account.
 */

#include "ABC_Login.h"
#include "ABC_LoginDir.h"
#include "ABC_LoginServer.h"
#include "ABC_Account.h"
#include "util/ABC_Crypto.h"
#include <ctype.h>

#define ACCOUNT_MK_LENGTH 32

static tABC_CC ABC_LoginFixUserName(const char *szUserName, char **pszOut, tABC_Error *pError);

/**
 * Deletes a login object and all its contents.
 */
void ABC_LoginFree(tABC_Login *pSelf)
{
    if (pSelf)
    {
        ABC_FREE_STR(pSelf->szUserName);
        ABC_BUF_FREE(pSelf->L1);

        ABC_BUF_FREE(pSelf->MK);
        ABC_FREE_STR(pSelf->szSyncKey);

        ABC_CLEAR_FREE(pSelf, sizeof(tABC_Login));
    }
}

/**
 * Sets up the username and L1 parameters in the login object.
 */
tABC_CC ABC_LoginNew(tABC_Login **ppSelf,
                     const char *szUserName,
                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_CryptoSNRP *pSNRP0 = NULL;
    tABC_Login *pSelf = NULL;
    ABC_ALLOC(pSelf, sizeof(tABC_Login));

    // Set up identity:
    ABC_CHECK_RET(ABC_LoginFixUserName(szUserName, &pSelf->szUserName, pError));
    ABC_CHECK_RET(ABC_LoginDirGetNumber(szUserName, &pSelf->AccountNum, pError));

    // Create L1:
    tABC_U08Buf L = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(L, (unsigned char *)pSelf->szUserName, strlen(pSelf->szUserName));
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForServer(&pSNRP0, pError));
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(L, pSNRP0, &pSelf->L1, pError));

    *ppSelf = pSelf;
    pSelf = NULL;

exit:
    if (pSelf)          ABC_LoginFree(pSelf);
    if (pSNRP0)         ABC_CryptoFreeSNRP(&pSNRP0);

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
    tABC_U08Buf         SyncKey         = ABC_BUF_NULL;
    tABC_U08Buf         LP              = ABC_BUF_NULL;
    tABC_U08Buf         LP1             = ABC_BUF_NULL;
    tABC_U08Buf         LP2             = ABC_BUF_NULL;
    tABC_CarePackage    *pCarePackage   = NULL;
    tABC_LoginPackage   *pLoginPackage  = NULL;

    // Allocate self:
    ABC_CHECK_RET(ABC_LoginNew(&pSelf, szUserName, pError));

    // Set up packages:
    ABC_CHECK_RET(ABC_CarePackageNew(&pCarePackage, pError));
    ABC_ALLOC(pLoginPackage, sizeof(tABC_LoginPackage));

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
    ABC_CHECK_RET(ABC_LoginDirCreate(&pSelf->AccountNum, pSelf->szUserName, pError));
    ABC_CHECK_RET(ABC_LoginDirSavePackages(pSelf->AccountNum, pCarePackage, pLoginPackage, pError));

    // Assign the final output:
    *ppSelf = pSelf;
    pSelf = NULL;

exit:
    ABC_LoginFree(pSelf);
    ABC_BUF_FREE(SyncKey);
    ABC_BUF_FREE(LP);
    ABC_BUF_FREE(LP1);
    ABC_BUF_FREE(LP2);
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
    tABC_SyncKeys *pKeys = NULL;

    ABC_ALLOC(pKeys, sizeof(tABC_SyncKeys));
    ABC_CHECK_RET(ABC_LoginGetSyncDirName(pSelf->szUserName, &pKeys->szSyncDir, pError));
    ABC_BUF_DUP(pKeys->MK, pSelf->MK);
    ABC_STRDUP(pKeys->szSyncKey, pSelf->szSyncKey);

    *ppKeys = pKeys;
    pKeys = NULL;

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);
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

    ABC_CHECK_RET(ABC_LoginDirLoadPackages(pSelf->AccountNum, &pCarePackage, &pLoginPackage, pError));
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
static
tABC_CC ABC_LoginFixUserName(const char *szUserName,
                             char **pszOut,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szOut = malloc(strlen(szUserName) + 1);
    ABC_CHECK_NULL(szOut);

    const char *si = szUserName;
    char *di = szOut;

    // Collapse leading & internal spaces:
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
