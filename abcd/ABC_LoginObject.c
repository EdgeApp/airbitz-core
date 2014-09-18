/**
 * @file
 * An object representing a logged-in account.
 */

#include "ABC_LoginObject.h"
#include "ABC_LoginDir.h"
#include "ABC_LoginServer.h"
#include "ABC_Account.h"
#include "util/ABC_Crypto.h"
#include <ctype.h>

#define ACCOUNT_MK_LENGTH 32

// CarePackage.json:
#define JSON_ACCT_SNRP2_FIELD                   "SNRP2"
#define JSON_ACCT_SNRP3_FIELD                   "SNRP3"
#define JSON_ACCT_SNRP4_FIELD                   "SNRP4"
#define JSON_ACCT_ERQ_FIELD                     "ERQ"

// LoginPackage.json:
#define JSON_ACCT_EMK_LP2_FIELD                 "EMK_LP2"
#define JSON_ACCT_EMK_LRA3_FIELD                "EMK_LRA3"
#define JSON_ACCT_ESYNCKEY_FIELD                "ESyncKey"
#define JSON_ACCT_ELP1_FIELD                    "ELP1"
#define JSON_ACCT_ELRA1_FIELD                   "ELRA1"

struct sABC_LoginObject
{
    // Identity:
    char            *szUserName;
    int             AccountNum;

    // Crypto settings:
    tABC_CryptoSNRP *pSNRP1;
    tABC_CryptoSNRP *pSNRP2;
    tABC_CryptoSNRP *pSNRP3;
    tABC_CryptoSNRP *pSNRP4;

    // Login server keys:
    tABC_U08Buf     L1;
    tABC_U08Buf     LP1;
    tABC_U08Buf     LRA1;       // Optional

    // Recovery:
    tABC_U08Buf     L4;
    tABC_U08Buf     RQ;         // Optional

    // Account access:
    tABC_U08Buf     MK;
    tABC_U08Buf     SyncKey;
    char            *szSyncKey; // Hex-encoded

    // Encrypted MK's:
    json_t          *EMK_LP2;
    json_t          *EMK_LRA3;  // Optional
};

typedef enum
{
    ABC_LP2,
    ABC_LRA3
} tABC_KeyType;

static tABC_CC ABC_LoginObjectFixUserName(const char *szUserName, char **pszOut, tABC_Error *pError);
static tABC_CC ABC_LoginObjectSetupUser(tABC_LoginObject *pSelf, const char *szUserName, tABC_Error *pError);
static tABC_CC ABC_LoginObjectLoadCarePackage(tABC_LoginObject *pSelf, tABC_Error *pError);
static tABC_CC ABC_LoginObjectLoadLoginPackage(tABC_LoginObject *pSelf, tABC_KeyType type, tABC_U08Buf Key, tABC_Error *pError);
static tABC_CC ABC_LoginObjectWriteCarePackage(tABC_LoginObject *pSelf, char **pszCarePackage, tABC_Error *pError);
static tABC_CC ABC_LoginObjectWriteLoginPackage(tABC_LoginObject *pSelf, char **pszLoginPackage, tABC_Error *pError);

/**
 * Deletes a login object and all its contents.
 */
void ABC_LoginObjectFree(tABC_LoginObject *pSelf)
{
    if (pSelf)
    {
        ABC_FREE_STR(pSelf->szUserName);

        ABC_CryptoFreeSNRP(&pSelf->pSNRP1);
        ABC_CryptoFreeSNRP(&pSelf->pSNRP2);
        ABC_CryptoFreeSNRP(&pSelf->pSNRP3);
        ABC_CryptoFreeSNRP(&pSelf->pSNRP4);

        ABC_BUF_FREE(pSelf->L1);
        ABC_BUF_FREE(pSelf->LP1);
        ABC_BUF_FREE(pSelf->LRA1);

        ABC_BUF_FREE(pSelf->L4);
        ABC_BUF_FREE(pSelf->RQ);

        ABC_BUF_FREE(pSelf->MK);
        ABC_BUF_FREE(pSelf->SyncKey);
        ABC_FREE_STR(pSelf->szSyncKey);

        if (pSelf->EMK_LP2)  json_decref(pSelf->EMK_LP2);
        if (pSelf->EMK_LRA3) json_decref(pSelf->EMK_LRA3);

        ABC_CLEAR_FREE(pSelf, sizeof(tABC_LoginObject));
    }
}

/**
 * Creates a new login account, both on-disk and on the server.
 *
 * @param szUserName    The user name for the account.
 * @param szPassword    The password for the account.
 * @param ppSelf        The returned login object.
 */
tABC_CC ABC_LoginObjectCreate(const char *szUserName,
                              const char *szPassword,
                              tABC_LoginObject **ppSelf,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_LoginObject    *pSelf          = NULL;
    tABC_U08Buf         LP              = ABC_BUF_NULL;
    tABC_U08Buf         LP2             = ABC_BUF_NULL;
    char                *szCarePackage  = NULL;
    char                *szLoginPackage = NULL;
    tABC_SyncKeys       *pSyncKeys      = NULL;

    // Allocate self:
    ABC_ALLOC(pSelf, sizeof(tABC_LoginObject));
    ABC_CHECK_RET(ABC_LoginObjectSetupUser(pSelf, szUserName, pError));
    if (0 <= pSelf->AccountNum)
    {
        ABC_RET_ERROR(ABC_CC_AccountAlreadyExists, "Account already exists");
    }

    // Generate SNRP's:
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&pSelf->pSNRP2, pError));
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&pSelf->pSNRP3, pError));
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&pSelf->pSNRP4, pError));

    // L4  = Scrypt(L, SNRP4):
    tABC_U08Buf L = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(L, (unsigned char *)pSelf->szUserName, strlen(pSelf->szUserName));
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(L, pSelf->pSNRP4, &pSelf->L4, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, pSelf->szUserName, szPassword);

    // LP1 = Scrypt(LP, SNRP1):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSelf->pSNRP1, &pSelf->LP1, pError));

    // Generate MK:
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(ACCOUNT_MK_LENGTH, &pSelf->MK, pError));

    // Generate SyncKey:
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(SYNC_KEY_LENGTH, &pSelf->SyncKey, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(pSelf->SyncKey, &pSelf->szSyncKey, pError));

    // EMK_LP2 = AES256(MK, Scrypt(LP, SNRP2))
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSelf->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->MK, LP2,
        ABC_CryptoType_AES256, &pSelf->EMK_LP2, pError));

    // At this point, the login object is fully-formed in memory!
    // Now we need to save it to disk and upload it to the server:
    ABC_CHECK_RET(ABC_LoginObjectWriteCarePackage(pSelf, &szCarePackage, pError));
    ABC_CHECK_RET(ABC_LoginObjectWriteLoginPackage(pSelf, &szLoginPackage, pError));

    // Create the account and repo on server:
    ABC_CHECK_RET(ABC_LoginServerCreate(pSelf->L1, pSelf->LP1,
        szCarePackage, szLoginPackage, pSelf->szSyncKey, pError));

    // Create the account and repo on disk:
    ABC_CHECK_RET(ABC_LoginDirCreate(pSelf->szUserName,
        szCarePackage, szLoginPackage, pError));
    ABC_CHECK_RET(ABC_LoginDirGetNumber(pSelf->szUserName, &pSelf->AccountNum, pError));

    // Populate the sync dir with files:
    ABC_CHECK_RET(ABC_LoginObjectGetSyncKeys(pSelf, &pSyncKeys, pError));
    ABC_CHECK_RET(ABC_AccountCreate(pSyncKeys, pError));

    // Upload the sync dir:
    int dirty;
    ABC_CHECK_RET(ABC_LoginObjectSync(pSelf, &dirty, pError));

    // Latch the account:
    ABC_CHECK_RET(ABC_LoginServerActivate(pSelf->L1, pSelf->LP1, pError));

    // Assign the final output:
    *ppSelf = pSelf;
    pSelf = NULL;

exit:
    ABC_LoginObjectFree(pSelf);
    ABC_BUF_FREE(LP);
    ABC_BUF_FREE(LP2);
    ABC_FREE_STR(szCarePackage);
    ABC_FREE_STR(szLoginPackage);
    ABC_SyncFreeKeys(pSyncKeys);
    return cc;
}

/**
 * Loads an existing login object, either from the server or from disk.
 *
 * @param szUserName    The user name for the account.
 * @param szPassword    The password for the account.
 * @param ppSelf        The returned login object.
 */
tABC_CC ABC_LoginObjectFromPassword(const char *szUserName,
                                    const char *szPassword,
                                    tABC_LoginObject **ppSelf,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_LoginObject    *pSelf          = NULL;
    tABC_U08Buf         LP              = ABC_BUF_NULL;
    tABC_U08Buf         LP2             = ABC_BUF_NULL;

    // Allocate self:
    ABC_ALLOC(pSelf, sizeof(tABC_LoginObject));
    ABC_CHECK_RET(ABC_LoginObjectSetupUser(pSelf, szUserName, pError));

    // Load CarePackage:
    ABC_CHECK_RET(ABC_LoginObjectLoadCarePackage(pSelf, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, pSelf->szUserName, szPassword);

    // LP1 = Scrypt(LP, SNRP1):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSelf->pSNRP1, &pSelf->LP1, pError));

    // Load the login package using LP2 = Scrypt(LP, SNRP2):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSelf->pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_LoginObjectLoadLoginPackage(pSelf, ABC_LP2, LP2, pError));

    // At this point, the login object is fully-formed in memory!
    // Now we need to sync with the server:
    int dirty;
    ABC_CHECK_RET(ABC_LoginObjectSync(pSelf, &dirty, pError));

    // Assign the final output:
    *ppSelf = pSelf;
    pSelf = NULL;

exit:
    ABC_LoginObjectFree(pSelf);
    ABC_BUF_FREE(LP);
    ABC_BUF_FREE(LP2);
    return cc;
}

/**
 * Loads an existing login object, either from the server or from disk.
 * Uses recovery answers rather than a password.
 *
 * @param szUserName    The user name for the account.
 * @param szPassword    The password for the account.
 * @param ppSelf        The returned login object.
 */
tABC_CC ABC_LoginObjectFromRecovery(const char *szUserName,
                                    const char *szRecoveryAnswers,
                                    tABC_LoginObject **ppSelf,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_LoginObject    *pSelf          = NULL;
    tABC_U08Buf         LRA             = ABC_BUF_NULL;
    tABC_U08Buf         LRA3            = ABC_BUF_NULL;

    // Allocate self:
    ABC_ALLOC(pSelf, sizeof(tABC_LoginObject));
    ABC_CHECK_RET(ABC_LoginObjectSetupUser(pSelf, szUserName, pError));

    // Load CarePackage:
    ABC_CHECK_RET(ABC_LoginObjectLoadCarePackage(pSelf, pError));

    // LRA = L + RA:
    ABC_BUF_STRCAT(LRA, pSelf->szUserName, szRecoveryAnswers);

    // LRA1 = Scrypt(LRA, SNRP1):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pSelf->pSNRP1, &pSelf->LRA1, pError));

    // Load the login package using LRA3 = Scrypt(LRA, SNRP3):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pSelf->pSNRP3, &LRA3, pError));
    ABC_CHECK_RET(ABC_LoginObjectLoadLoginPackage(pSelf, ABC_LRA3, LRA3, pError));

    // At this point, the login object is fully-formed in memory!
    // Now we need to sync with the server:
    int dirty;
    ABC_CHECK_RET(ABC_LoginObjectSync(pSelf, &dirty, pError));

    // Assign the final output:
    *ppSelf = pSelf;
    pSelf = NULL;

exit:
    ABC_LoginObjectFree(pSelf);
    ABC_BUF_FREE(LRA);
    ABC_BUF_FREE(LRA3);
    return cc;
}

/**
 * Syncs the repository with the server.
 * @param pSelf         An already-loaded login object.
 */
tABC_CC ABC_LoginObjectSync(tABC_LoginObject *pSelf,
                            int *pDirty,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char            *szCarePackage  = NULL;
    char            *szLoginPackage = NULL;
    tABC_SyncKeys   *pKeys          = NULL;

    // Create the directory if it does not exist:
    if (pSelf->AccountNum < 0)
    {
        ABC_CHECK_RET(ABC_LoginObjectWriteCarePackage(pSelf, &szCarePackage, pError));
        ABC_CHECK_RET(ABC_LoginObjectWriteLoginPackage(pSelf, &szLoginPackage, pError));

        // Create the account directory:
        ABC_CHECK_RET(ABC_LoginDirCreate(pSelf->szUserName,
            szCarePackage, szLoginPackage, pError));
        ABC_CHECK_RET(ABC_LoginDirGetNumber(pSelf->szUserName, &pSelf->AccountNum, pError));
    }

    // Now do the sync:
    ABC_CHECK_RET(ABC_LoginObjectGetSyncKeys(pSelf, &pKeys, pError));
    ABC_CHECK_RET(ABC_SyncRepo(pKeys->szSyncDir, pKeys->szSyncKey, pDirty, pError));

exit:
    ABC_FREE_STR(szCarePackage);
    ABC_FREE_STR(szLoginPackage);
    if (pKeys)          ABC_SyncFreeKeys(pKeys);
    return cc;
}

tABC_CC ABC_LoginObjectUpdateLoginPackage(tABC_LoginObject *pSelf,
                                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szLoginPackage = NULL;

    ABC_CHECK_RET(ABC_LoginServerGetLoginPackage(pSelf->L1, pSelf->LP1, pSelf->LRA1, &szLoginPackage, pError));

exit:
    ABC_FREE_STR(szLoginPackage);
    return cc;
}

/**
 * Changes the password on an existing login object.
 * @param pSelf         An already-loaded login object.
 * @param szPassword    The new password.
 */
tABC_CC ABC_LoginObjectSetPassword(tABC_LoginObject *pSelf,
                                   const char *szPassword,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_CryptoSNRP *pSNRP2 = NULL;
    tABC_U08Buf LP          = ABC_BUF_NULL;
    tABC_U08Buf LP1         = ABC_BUF_NULL;
    tABC_U08Buf LP2         = ABC_BUF_NULL;
    json_t *EMK_LP2         = NULL;
    char *szCarePackage     = NULL;
    char *szLoginPackage    = NULL;

    // Update scrypt parameters:
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&pSNRP2, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, pSelf->szUserName, szPassword);

    // LP1 = Scrypt(LP, SNRP1):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSelf->pSNRP1, &LP1, pError));

    // EMK_LP2 = AES256(MK, Scrypt(LP, SNRP2)):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSNRP2, &LP2, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->MK, LP2,
        ABC_CryptoType_AES256, &EMK_LP2, pError));

    // At this point, we have all the new stuff sitting in memory!

    // Write new packages:
    tABC_LoginObject temp = *pSelf;
    temp.pSNRP2 = pSNRP2;
    temp.LP1 = LP1;
    temp.EMK_LP2 = EMK_LP2;
    ABC_CHECK_RET(ABC_LoginObjectWriteCarePackage(&temp, &szCarePackage, pError));
    ABC_CHECK_RET(ABC_LoginObjectWriteLoginPackage(&temp, &szLoginPackage, pError));

    // Change the server login:
    ABC_CHECK_RET(ABC_LoginServerChangePassword(pSelf->L1, pSelf->LP1, pSelf->LRA1,
        temp.LP1, temp.LRA1, szCarePackage, szLoginPackage, pError));

    // It's official now, so update pSelf:
    ABC_SWAP(pSelf->pSNRP2, pSNRP2);
    ABC_BUF_SWAP(pSelf->LP1, LP1);
    ABC_SWAP(pSelf->EMK_LP2, EMK_LP2);

    // Change the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirFileSave(szCarePackage, pSelf->AccountNum, ACCOUNT_CARE_PACKAGE_FILENAME, pError));
    ABC_CHECK_RET(ABC_LoginDirFileSave(szLoginPackage, pSelf->AccountNum, ACCOUNT_LOGIN_PACKAGE_FILENAME, pError));

exit:
    ABC_CryptoFreeSNRP(&pSNRP2);
    ABC_BUF_FREE(LP);
    ABC_BUF_FREE(LP1);
    ABC_BUF_FREE(LP2);
    if (EMK_LP2) json_decref(EMK_LP2);
    ABC_FREE_STR(szCarePackage);
    ABC_FREE_STR(szLoginPackage);

    return cc;
}

/**
 * Changes the recovery questions and answers on an existing login object.
 * @param pSelf         An already-loaded login object.
 */
tABC_CC ABC_LoginObjectSetRecovery(tABC_LoginObject *pSelf,
                                   const char *szRecoveryQuestions,
                                   const char *szRecoveryAnswers,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_CryptoSNRP *pSNRP3 = NULL;
    tABC_CryptoSNRP *pSNRP4 = NULL;
    tABC_U08Buf L4          = ABC_BUF_NULL;
    tABC_U08Buf RQ          = ABC_BUF_NULL;
    tABC_U08Buf LRA         = ABC_BUF_NULL;
    tABC_U08Buf LRA1        = ABC_BUF_NULL;
    tABC_U08Buf LRA3        = ABC_BUF_NULL;
    json_t *EMK_LRA3        = NULL;
    char *szCarePackage     = NULL;
    char *szLoginPackage    = NULL;

    // Update scrypt parameters:
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&pSNRP3, pError));
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForClient(&pSNRP4, pError));

    // L4  = Scrypt(L, SNRP4):
    tABC_U08Buf L = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(L, (unsigned char *)pSelf->szUserName, strlen(pSelf->szUserName));
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(L, pSNRP4, &L4, pError));

    // RQ = recovery questions:
    ABC_BUF_DUP_PTR(RQ, (unsigned char *)szRecoveryQuestions, strlen(szRecoveryQuestions) + 1);

    // LRA = L + RA:
    ABC_BUF_STRCAT(LRA, pSelf->szUserName, szRecoveryAnswers);

    // LRA1 = Scrypt(LRA, SNRP1):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pSelf->pSNRP1, &LRA1, pError));

    // EMK_LRA3 = AES256(MK, Scrypt(LRA, SNRP3)):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pSNRP3, &LRA3, pError));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->MK, LRA3,
        ABC_CryptoType_AES256, &EMK_LRA3, pError));

    // At this point, we have all the new stuff sitting in memory!

    // Write new packages:
    tABC_LoginObject temp = *pSelf;
    temp.pSNRP3     = pSNRP3;
    temp.pSNRP4     = pSNRP4;
    temp.L4         = L4;
    temp.RQ         = RQ;
    temp.LRA1       = LRA1;
    temp.EMK_LRA3   = EMK_LRA3;
    ABC_CHECK_RET(ABC_LoginObjectWriteCarePackage(&temp, &szCarePackage, pError));
    ABC_CHECK_RET(ABC_LoginObjectWriteLoginPackage(&temp, &szLoginPackage, pError));

    // Change the server login:
    ABC_CHECK_RET(ABC_LoginServerChangePassword(pSelf->L1, pSelf->LP1, pSelf->LRA1,
        temp.LP1, temp.LRA1, szCarePackage, szLoginPackage, pError));

    // It's official now, so update pSelf:
    ABC_SWAP(pSelf->pSNRP3,     pSNRP3);
    ABC_SWAP(pSelf->pSNRP4,     pSNRP4);
    ABC_BUF_SWAP(pSelf->L4,     L4);
    ABC_BUF_SWAP(pSelf->RQ,     RQ);
    ABC_BUF_SWAP(pSelf->LRA1,   LRA1);
    ABC_SWAP(pSelf->EMK_LRA3,   EMK_LRA3);

    // Change the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirFileSave(szCarePackage, pSelf->AccountNum, ACCOUNT_CARE_PACKAGE_FILENAME, pError));
    ABC_CHECK_RET(ABC_LoginDirFileSave(szLoginPackage, pSelf->AccountNum, ACCOUNT_LOGIN_PACKAGE_FILENAME, pError));

exit:
    ABC_CryptoFreeSNRP(&pSNRP3);
    ABC_CryptoFreeSNRP(&pSNRP4);
    ABC_BUF_FREE(L4);
    ABC_BUF_FREE(RQ);
    ABC_BUF_FREE(LRA);
    ABC_BUF_FREE(LRA1);
    ABC_BUF_FREE(LRA3);
    if (EMK_LRA3) json_decref(EMK_LRA3);
    ABC_FREE_STR(szCarePackage);
    ABC_FREE_STR(szLoginPackage);

    return cc;
}

/**
 * Determines whether or not the given string matches the account's
 * username.
 * @param szUserName    The user name to check.
 * @param pMatch        Set to 1 if the names match.
 */
tABC_CC ABC_LoginObjectCheckUserName(tABC_LoginObject *pSelf,
                                     const char *szUserName,
                                     int *pMatch,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFixed = NULL;
    *pMatch = 0;

    ABC_CHECK_RET(ABC_LoginObjectFixUserName(szUserName, &szFixed, pError));
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
tABC_CC ABC_LoginObjectGetSyncKeys(tABC_LoginObject *pSelf,
                                   tABC_SyncKeys **ppKeys,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_SyncKeys *pKeys = NULL;

    ABC_ALLOC(pKeys, sizeof(tABC_SyncKeys));
    ABC_CHECK_RET(ABC_LoginGetSyncDirName(pSelf->szUserName, &pKeys->szSyncDir, pError));
    ABC_BUF_SET(pKeys->MK, pSelf->MK);
    pKeys->szSyncKey = pSelf->szSyncKey;

    *ppKeys = pKeys;
    pKeys = NULL;

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);
    return cc;
}

/**
 * Obtains an account object's user name.
 * @param pL1       The hashed user name. Do *not* free this.
 * @param pLP1      The hashed user name & password. Do *not* free this.
 */
tABC_CC ABC_LoginObjectGetServerKeys(tABC_LoginObject *pSelf,
                                     tABC_U08Buf *pL1,
                                     tABC_U08Buf *pLP1,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_CHECK_NULL(pSelf);

    ABC_BUF_SET(*pL1, pSelf->L1);
    ABC_BUF_SET(*pLP1, pSelf->LP1);

exit:
    return cc;
}

/**
 * Obtains the recovery questions for a user.
 *
 * @param pszRecoveryQuestions The returned questions. The caller frees this.
 */
tABC_CC ABC_LoginObjectGetRQ(const char *szUserName,
                             char **pszRecoveryQuestions,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_LoginObject    *pSelf  = NULL;

    // Allocate self:
    ABC_ALLOC(pSelf, sizeof(tABC_LoginObject));
    ABC_CHECK_RET(ABC_LoginObjectSetupUser(pSelf, szUserName, pError));

    // Load CarePackage:
    ABC_CHECK_RET(ABC_LoginObjectLoadCarePackage(pSelf, pError));
    ABC_CHECK_ASSERT(ABC_BUF_PTR(pSelf->RQ), ABC_CC_NoRecoveryQuestions, "No recovery questions");

    // Write the output:
    ABC_STRDUP(*pszRecoveryQuestions, (char *)ABC_BUF_PTR(pSelf->RQ));

exit:
    ABC_LoginObjectFree(pSelf);

    return cc;
}

/**
 * Re-formats a username to all-lowercase, checking for disallowed
 * characters and collapsing spaces.
 */
static
tABC_CC ABC_LoginObjectFixUserName(const char *szUserName,
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

/**
 * Sets up the username and L1 parameters in the login object.
 */
static
tABC_CC ABC_LoginObjectSetupUser(tABC_LoginObject *pSelf,
                                 const char *szUserName,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    // Set up identity:
    ABC_CHECK_RET(ABC_LoginObjectFixUserName(szUserName, &pSelf->szUserName, pError));
    ABC_CHECK_RET(ABC_LoginDirGetNumber(szUserName, &pSelf->AccountNum, pError));

    // Load SRNP1:
    ABC_CHECK_RET(ABC_CryptoCreateSNRPForServer(&pSelf->pSNRP1, pError));

    // Create L1:
    tABC_U08Buf L = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(L, (unsigned char *)pSelf->szUserName, strlen(pSelf->szUserName));
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(L, pSelf->pSNRP1, &pSelf->L1, pError));

exit:
    return cc;
}

/**
 * Loads the CarePackage into a nascent login object,
 * either from disk or from the server.
 */
static
tABC_CC ABC_LoginObjectLoadCarePackage(tABC_LoginObject *pSelf,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char    *szCarePackage  = NULL;
    json_t  *pJSON_Root     = NULL;
    json_t  *pJSON_SNRP2    = NULL;
    json_t  *pJSON_SNRP3    = NULL;
    json_t  *pJSON_SNRP4    = NULL;
    json_t  *pJSON_ERQ      = NULL;
    int     e;

    // Fetch the package from the server:
    cc = ABC_LoginServerGetCarePackage(pSelf->L1, &szCarePackage, pError);

    // If that didn't work, load the package from disk:
    if (cc != ABC_CC_Ok && 0 <= pSelf->AccountNum)
    {
        tABC_Error error;
        ABC_LoginDirFileLoad(&szCarePackage,
            pSelf->AccountNum, ACCOUNT_CARE_PACKAGE_FILENAME, &error);
    }

    // If that didn't work, fetch the package from the server:
    if (!szCarePackage)
        goto exit;

    // Parse the JSON:
    json_error_t error;
    pJSON_Root = json_loads(szCarePackage, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing CarePackage JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing CarePackage JSON");

    // Unpack the contents:
    e = json_unpack(pJSON_Root, "{s:o, s:o, s:o, s?o}",
                    JSON_ACCT_SNRP2_FIELD, &pJSON_SNRP2,
                    JSON_ACCT_SNRP3_FIELD, &pJSON_SNRP3,
                    JSON_ACCT_SNRP4_FIELD, &pJSON_SNRP4,
                    JSON_ACCT_ERQ_FIELD,   &pJSON_ERQ);
    ABC_CHECK_SYS(!e, "Error parsing CarePackage JSON");

    // Decode SNRP's:
    ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP2, &pSelf->pSNRP2, pError));
    ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP3, &pSelf->pSNRP3, pError));
    ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(pJSON_SNRP4, &pSelf->pSNRP4, pError));

    // Create L4:
    tABC_U08Buf L = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(L, (unsigned char *)pSelf->szUserName, strlen(pSelf->szUserName));
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(L, pSelf->pSNRP4, &pSelf->L4, pError));

    // Get the ERQ (if any):
    if (pJSON_ERQ && json_is_object(pJSON_ERQ))
    {
        ABC_CryptoDecryptJSONObject(pJSON_ERQ, pSelf->L4, &pSelf->RQ, pError);
    }

exit:
    ABC_FREE_STR(szCarePackage);
    if (pJSON_Root)     json_decref(pJSON_Root);
    return cc;
}

/**
 * Loads the LoginPackage into a nascent login object,
 * either from disk or from the server.
 */
static
tABC_CC ABC_LoginObjectLoadLoginPackage(tABC_LoginObject *pSelf,
                                        tABC_KeyType type,
                                        tABC_U08Buf Key,
                                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char    *szLoginPackage = NULL;
    json_t  *pJSON_Root     = NULL;
    json_t  *pJSON_ESyncKey  = NULL;
    json_t  *pJSON_ELP1     = NULL;
    json_t  *pJSON_ELRA1    = NULL;
    int     e;

    // Fetch the package from the server:
    cc = ABC_LoginServerGetLoginPackage(pSelf->L1,
        pSelf->LP1, pSelf->LRA1, &szLoginPackage, pError);

    // If that didn't work, load the package from disk:
    if (cc != ABC_CC_Ok && 0 <= pSelf->AccountNum)
    {
        tABC_Error error;
        ABC_LoginDirFileLoad(&szLoginPackage,
            pSelf->AccountNum, ACCOUNT_LOGIN_PACKAGE_FILENAME, &error);
    }

    // If that didn't work, we have an error:
    if (!szLoginPackage)
        goto exit;

    // Parse the JSON:
    json_error_t error;
    pJSON_Root = json_loads(szLoginPackage, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing LoginPackage JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing LoginPackage JSON");

    // Unpack the contents:
    e = json_unpack(pJSON_Root, "{s?o, s?o, s:o, s?o, s?o}",
                    JSON_ACCT_EMK_LP2_FIELD,     &pSelf->EMK_LP2,
                    JSON_ACCT_EMK_LRA3_FIELD,    &pSelf->EMK_LRA3,
                    JSON_ACCT_ESYNCKEY_FIELD,    &pJSON_ESyncKey,
                    JSON_ACCT_ELP1_FIELD,        &pJSON_ELP1,
                    JSON_ACCT_ELRA1_FIELD,       &pJSON_ELRA1);
    ABC_CHECK_SYS(!e, "Error parsing LoginPackage JSON");

    // Save the EMK's:
    if (pSelf->EMK_LP2)     json_incref(pSelf->EMK_LP2);
    if (pSelf->EMK_LRA3)    json_incref(pSelf->EMK_LRA3);

    // Decrypt MK one way or the other:
    switch (type)
    {
    case ABC_LP2:
        ABC_CHECK_ASSERT(pSelf->EMK_LP2, ABC_CC_DecryptFailure, "Cannot decrypt login package - missing EMK_LP2");
        ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pSelf->EMK_LP2, Key, &pSelf->MK, pError));
        break;
    case ABC_LRA3:
        ABC_CHECK_ASSERT(pSelf->EMK_LRA3, ABC_CC_DecryptFailure, "Cannot decrypt login package - missing EMK_LRA3");
        ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pSelf->EMK_LRA3, Key, &pSelf->MK, pError));
        break;
    }

    // Decrypt SyncKey:
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pJSON_ESyncKey, pSelf->MK, &pSelf->SyncKey, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(pSelf->SyncKey, &pSelf->szSyncKey, pError));

    // Decrypt server keys:
    if (pJSON_ELP1 && !ABC_BUF_PTR(pSelf->LP1))
    {
        ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pJSON_ELP1, pSelf->MK, &pSelf->LP1, pError));
    }
    if (pJSON_ELRA1 && !ABC_BUF_PTR(pSelf->LRA1))
    {
        ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pJSON_ELRA1, pSelf->MK, &pSelf->LRA1, pError));
    }

exit:
    ABC_FREE_STR(szLoginPackage);
    if (pJSON_Root)     json_decref(pJSON_Root);
    return cc;
}

/**
 * Serializes the CarePackage objects to a JSON string.
 */
static
tABC_CC ABC_LoginObjectWriteCarePackage(tABC_LoginObject *pSelf,
                                        char **pszCarePackage,
                                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_t *pJSON_Root  = NULL;
    json_t *pJSON_SNRP2 = NULL;
    json_t *pJSON_SNRP3 = NULL;
    json_t *pJSON_SNRP4 = NULL;
    json_t *pJSON_ERQ   = NULL;

    // Build the SNRP's:
    ABC_CHECK_RET(ABC_CryptoCreateJSONObjectSNRP(pSelf->pSNRP2, &pJSON_SNRP2, pError));
    ABC_CHECK_RET(ABC_CryptoCreateJSONObjectSNRP(pSelf->pSNRP3, &pJSON_SNRP3, pError));
    ABC_CHECK_RET(ABC_CryptoCreateJSONObjectSNRP(pSelf->pSNRP4, &pJSON_SNRP4, pError));

    // Build the main body:
    pJSON_Root = json_pack("{s:o, s:o, s:o}",
        JSON_ACCT_SNRP2_FIELD, pJSON_SNRP2,
        JSON_ACCT_SNRP3_FIELD, pJSON_SNRP3,
        JSON_ACCT_SNRP4_FIELD, pJSON_SNRP4);

    // Build the ERQ, if any:
    if (ABC_BUF_SIZE(pSelf->RQ))
    {
        ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->RQ, pSelf->L4,
            ABC_CryptoType_AES256, &pJSON_ERQ, pError));
        json_object_set(pJSON_Root, JSON_ACCT_ERQ_FIELD, pJSON_ERQ);
    }

    // Write out:
    *pszCarePackage = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    ABC_CHECK_NULL(*pszCarePackage);

exit:
    if (pJSON_Root)     json_decref(pJSON_Root);
    if (pJSON_SNRP2)    json_decref(pJSON_SNRP2);
    if (pJSON_SNRP3)    json_decref(pJSON_SNRP3);
    if (pJSON_SNRP4)    json_decref(pJSON_SNRP4);
    if (pJSON_ERQ)      json_decref(pJSON_ERQ);

    return cc;
}

/**
 * Serializes the CarePackage objects to a JSON string.
 */
static
tABC_CC ABC_LoginObjectWriteLoginPackage(tABC_LoginObject *pSelf,
                                         char **pszLoginPackage,
                                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_t  *pJSON_Root     = NULL;
    json_t  *pJSON_ESyncKey = NULL;
    json_t  *pJSON_ELP1     = NULL;
    json_t  *pJSON_ELRA1    = NULL;

    // Encrypt SyncKey:
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->SyncKey, pSelf->MK,
        ABC_CryptoType_AES256, &pJSON_ESyncKey, pError));

    // Build the main body:
    pJSON_Root = json_pack("{s:o}",
        JSON_ACCT_ESYNCKEY_FIELD,   pJSON_ESyncKey);

    // Write master keys:
    if (pSelf->EMK_LP2)
    {
        json_object_set(pJSON_Root, JSON_ACCT_EMK_LP2_FIELD, pSelf->EMK_LP2);
    }
    if (pSelf->EMK_LRA3)
    {
        json_object_set(pJSON_Root, JSON_ACCT_EMK_LRA3_FIELD, pSelf->EMK_LRA3);
    }

    // Write server keys:
    if (ABC_BUF_PTR(pSelf->LP1))
    {
        ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->LP1, pSelf->MK,
            ABC_CryptoType_AES256, &pJSON_ELP1, pError));
        json_object_set(pJSON_Root, JSON_ACCT_ELP1_FIELD, pJSON_ELP1);
    }
    if (ABC_BUF_PTR(pSelf->LRA1))
    {
        ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->LRA1, pSelf->MK,
            ABC_CryptoType_AES256, &pJSON_ELRA1, pError));
        json_object_set(pJSON_Root, JSON_ACCT_ELRA1_FIELD, pJSON_ELRA1);
    }

    // Write out:
    *pszLoginPackage = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    ABC_CHECK_NULL(*pszLoginPackage);

exit:
    if (pJSON_Root)     json_decref(pJSON_Root);
    if (pJSON_ESyncKey) json_decref(pJSON_ESyncKey);
    if (pJSON_ELP1)     json_decref(pJSON_ELP1);
    if (pJSON_ELRA1)    json_decref(pJSON_ELRA1);

    return cc;
}
