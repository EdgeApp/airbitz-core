/**
 * @file
 * An object representing a logged-in account.
 */

#include "ABC_LoginObject.h"
#include "ABC_LoginDir.h"
#include "ABC_LoginServer.h"
#include "ABC_Account.h"
#include "ABC_Crypto.h"

#define ACCOUNT_MK_LENGTH 32

// CarePackage.json:
#define JSON_ACCT_SNRP2_FIELD                   "SNRP2"
#define JSON_ACCT_SNRP3_FIELD                   "SNRP3"
#define JSON_ACCT_SNRP4_FIELD                   "SNRP4"
#define JSON_ACCT_ERQ_FIELD                     "ERQ"

// LoginPackage.json:
#define JSON_ACCT_MK_FIELD                      "MK"
#define JSON_ACCT_SYNCKEY_FIELD                 "SyncKey"
#define JSON_ACCT_ELP2_FIELD                    "ELP2"
#define JSON_ACCT_ELRA3_FIELD                   "ELRA3"

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
    tABC_U08Buf     LP1;        // Absent when logging in with LRA!
    tABC_U08Buf     LRA1;       // Optional

    // Recovery:
    tABC_U08Buf     L4;
    tABC_U08Buf     RQ;         // Optional

    // Account access:
    tABC_U08Buf     LP2;
    tABC_U08Buf     LRA3;       // Optional
    tABC_U08Buf     MK;
    tABC_U08Buf     SyncKey;
    char            *szSyncKey; // Hex-encoded
};

static tABC_CC ABC_LoginObjectSetupUser(tABC_LoginObject *pSelf, const char *szUserName, tABC_Error *pError);
static tABC_CC ABC_LoginObjectLoadCarePackage(tABC_LoginObject *pSelf, tABC_Error *pError);
static tABC_CC ABC_LoginObjectLoadLoginPackage(tABC_LoginObject *pSelf, tABC_Error *pError);
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

        ABC_BUF_FREE(pSelf->LP2);
        ABC_BUF_FREE(pSelf->LRA3);
        ABC_BUF_FREE(pSelf->MK);
        ABC_BUF_FREE(pSelf->SyncKey);
        ABC_FREE_STR(pSelf->szSyncKey);

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

    // LP2 = Scrypt(LP, SNRP2):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSelf->pSNRP2, &pSelf->LP2, pError));

    // Generate MK:
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(ACCOUNT_MK_LENGTH, &pSelf->MK, pError));

    // Generate SyncKey:
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(SYNC_KEY_LENGTH, &pSelf->SyncKey, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(pSelf->SyncKey, &pSelf->szSyncKey, pError));

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

    // Allocate self:
    ABC_ALLOC(pSelf, sizeof(tABC_LoginObject));
    ABC_CHECK_RET(ABC_LoginObjectSetupUser(pSelf, szUserName, pError));

    // Load CarePackage:
    ABC_CHECK_RET(ABC_LoginObjectLoadCarePackage(pSelf, pError));

    // LP = L + P:
    ABC_BUF_STRCAT(LP, pSelf->szUserName, szPassword);

    // LP1 = Scrypt(LP, SNRP1):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSelf->pSNRP1, &pSelf->LP1, pError));

    // LP2 = Scrypt(LP, SNRP2):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSelf->pSNRP2, &pSelf->LP2, pError));

    // Load the login package:
    ABC_CHECK_RET(ABC_LoginObjectLoadLoginPackage(pSelf, pError));

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

    // Allocate self:
    ABC_ALLOC(pSelf, sizeof(tABC_LoginObject));
    ABC_CHECK_RET(ABC_LoginObjectSetupUser(pSelf, szUserName, pError));

    // Load CarePackage:
    ABC_CHECK_RET(ABC_LoginObjectLoadCarePackage(pSelf, pError));

    // LRA = L + RA:
    ABC_BUF_STRCAT(LRA, pSelf->szUserName, szRecoveryAnswers);

    // LRA1 = Scrypt(LRA, SNRP1):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pSelf->pSNRP1, &pSelf->LRA1, pError));

    // LRA3 = Scrypt(LRA, SNRP3):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pSelf->pSNRP3, &pSelf->LRA3, pError));

    // Load the login package:
    ABC_CHECK_RET(ABC_LoginObjectLoadLoginPackage(pSelf, pError));

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
    ABC_CHECK_RET(ABC_LoginDirFileSave(szLoginPackage, pSelf->AccountNum, ACCOUNT_LOGIN_PACKAGE_FILENAME, pError));

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

    tABC_U08Buf LP          = ABC_BUF_NULL;
    tABC_U08Buf LP1         = ABC_BUF_NULL;
    tABC_U08Buf LP2         = ABC_BUF_NULL;
    char *szLoginPackage    = NULL;

    // LP = L + P:
    ABC_BUF_STRCAT(LP, pSelf->szUserName, szPassword);

    // LP1 = Scrypt(LP, SNRP1):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSelf->pSNRP1, &LP1, pError));

    // LP2 = Scrypt(LP, SNRP2):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LP, pSelf->pSNRP2, &LP2, pError));

    // At this point, we have all the new stuff sitting in memory!

    // Write new packages:
    tABC_LoginObject temp = *pSelf;
    temp.LP1 = LP1;
    temp.LP2 = LP2;
    ABC_CHECK_RET(ABC_LoginObjectWriteLoginPackage(&temp, &szLoginPackage, pError));

    // Change the server login:
    ABC_CHECK_RET(ABC_LoginServerChangePassword(temp.L1, pSelf->LP1, temp.LRA1,
        temp.LP1, szLoginPackage, pError));

    // It's official now, so update pSelf:
    ABC_BUF_SWAP(pSelf->LP1, LP1);
    ABC_BUF_SWAP(pSelf->LP2, LP2);

    // Change the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirFileSave(szLoginPackage, pSelf->AccountNum, ACCOUNT_LOGIN_PACKAGE_FILENAME, pError));

exit:
    ABC_BUF_FREE(LP);
    ABC_BUF_FREE(LP1);
    ABC_BUF_FREE(LP2);
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

    tABC_U08Buf RQ          = ABC_BUF_NULL;
    tABC_U08Buf LRA         = ABC_BUF_NULL;
    tABC_U08Buf LRA1        = ABC_BUF_NULL;
    tABC_U08Buf LRA3        = ABC_BUF_NULL;
    char *szCarePackage     = NULL;
    char *szLoginPackage    = NULL;

    // RQ = recovery questions:
    ABC_BUF_DUP_PTR(RQ, (unsigned char *)szRecoveryQuestions, strlen(szRecoveryQuestions) + 1);

    // LRA = L + RA:
    ABC_BUF_STRCAT(LRA, pSelf->szUserName, szRecoveryAnswers);

    // LRA1 = Scrypt(LRA, SNRP1):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pSelf->pSNRP1, &LRA1, pError));

    // LRA3 = Scrypt(LRA, SNRP3):
    ABC_CHECK_RET(ABC_CryptoScryptSNRP(LRA, pSelf->pSNRP3, &LRA3, pError));

    // At this point, we have all the new stuff sitting in memory!

    // Write new packages:
    tABC_LoginObject temp = *pSelf;
    temp.RQ   = RQ;
    temp.LRA1 = LRA1;
    temp.LRA3 = LRA3;
    ABC_CHECK_RET(ABC_LoginObjectWriteCarePackage(&temp, &szCarePackage, pError));
    ABC_CHECK_RET(ABC_LoginObjectWriteLoginPackage(&temp, &szLoginPackage, pError));

    // Change the server login:
    ABC_CHECK_RET(ABC_LoginServerSetRecovery(temp.L1, temp.LP1, temp.LRA1,
        szCarePackage, szLoginPackage, pError));

    // It's official now, so update pSelf:
    ABC_BUF_SWAP(pSelf->RQ,   RQ);
    ABC_BUF_SWAP(pSelf->LRA1, LRA1);
    ABC_BUF_SWAP(pSelf->LRA3, LRA3);

    // Change the on-disk login:
    ABC_CHECK_RET(ABC_LoginDirFileSave(szCarePackage, pSelf->AccountNum, ACCOUNT_CARE_PACKAGE_FILENAME, pError));
    ABC_CHECK_RET(ABC_LoginDirFileSave(szLoginPackage, pSelf->AccountNum, ACCOUNT_LOGIN_PACKAGE_FILENAME, pError));

exit:
    ABC_BUF_FREE(RQ);
    ABC_BUF_FREE(LRA);
    ABC_BUF_FREE(LRA1);
    ABC_BUF_FREE(LRA3);
    ABC_FREE_STR(szCarePackage);
    ABC_FREE_STR(szLoginPackage);

    return cc;
}

/**
 * Obtains an account object's user name.
 * @param pszUserName   The user name. Do *not* free this.
 */
tABC_CC ABC_LoginObjectGetUserName(tABC_LoginObject *pSelf,
                                   const char **pszUserName,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_CHECK_NULL(pSelf);

    *pszUserName = pSelf->szUserName;

exit:
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
    ABC_CHECK_NULL(pSelf);

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

    // Write the output:
    ABC_STRDUP(*pszRecoveryQuestions, (char *)ABC_BUF_PTR(pSelf->RQ));

exit:
    ABC_LoginObjectFree(pSelf);

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
    ABC_STRDUP(pSelf->szUserName, szUserName);
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

    // Load the package from disk:
    if (0 <= pSelf->AccountNum)
    {
        ABC_LoginDirFileLoad(&szCarePackage,
            pSelf->AccountNum, ACCOUNT_CARE_PACKAGE_FILENAME, pError);
    }

    // If that didn't work, fetch the package from the server:
    if (!szCarePackage)
    {
        ABC_CHECK_RET(ABC_LoginServerGetCarePackage(pSelf->L1, &szCarePackage, pError));
    }

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
        ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pJSON_ERQ, pSelf->L4, &pSelf->RQ, pError));
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
                                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char    *szLoginPackage = NULL;
    json_t  *pJSON_Root     = NULL;
    json_t  *pJSON_MK       = NULL;
    json_t  *pJSON_SyncKey  = NULL;
    json_t  *pJSON_ELP2     = NULL;
    json_t  *pJSON_ELRA3    = NULL;
    tABC_U08Buf SyncKey     = ABC_BUF_NULL;
    int     e;

    // Load the package from disk:
    if (0 <= pSelf->AccountNum)
    {
        ABC_LoginDirFileLoad(&szLoginPackage,
            pSelf->AccountNum, ACCOUNT_LOGIN_PACKAGE_FILENAME, pError);
    }

    // If that didn't work, fetch the package from the server:
    if (!szLoginPackage)
    {
        ABC_CHECK_RET(ABC_LoginServerGetLoginPackage(pSelf->L1,
            pSelf->LP1, pSelf->LRA1, &szLoginPackage, pError));
    }

    // Parse the JSON:
    json_error_t error;
    pJSON_Root = json_loads(szLoginPackage, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing LoginPackage JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing LoginPackage JSON");

    // Unpack the contents:
    e = json_unpack(pJSON_Root, "{s:o, s:o, s?o, s?o}",
                    JSON_ACCT_MK_FIELD,         &pJSON_MK,
                    JSON_ACCT_SYNCKEY_FIELD,    &pJSON_SyncKey,
                    JSON_ACCT_ELP2_FIELD,       &pJSON_ELP2,
                    JSON_ACCT_ELRA3_FIELD,      &pJSON_ELRA3);
    ABC_CHECK_SYS(!e, "Error parsing LoginPackage JSON");

    // Use one login key to gain access to the other:
    if (ABC_BUF_PTR(pSelf->LP2) && !ABC_BUF_PTR(pSelf->LRA3) &&
        pJSON_ELRA3 && json_is_object(pJSON_ELRA3))
    {
        ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pJSON_ELRA3, pSelf->LP2, &pSelf->LRA3, pError));
    }
    if (ABC_BUF_PTR(pSelf->LRA3) && !ABC_BUF_PTR(pSelf->LP2) &&
        pJSON_ELP2 && json_is_object(pJSON_ELP2))
    {
        ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pJSON_ELP2, pSelf->LRA3, &pSelf->LP2, pError));
    }
    ABC_CHECK_ASSERT(ABC_BUF_PTR(pSelf->LP2), ABC_CC_DecryptFailure, "Error loading LoginPackage - cannot get LP2");

    // Decrypt MK:
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pJSON_MK, pSelf->LP2, &pSelf->MK, pError));

    // Decrypt SyncKey:
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(pJSON_SyncKey, pSelf->L4, &SyncKey, pError));
    ABC_STRDUP(pSelf->szSyncKey, (char *)ABC_BUF_PTR(SyncKey));
    ABC_CHECK_RET(ABC_CryptoHexDecode(pSelf->szSyncKey, &pSelf->SyncKey, pError));

exit:
    ABC_FREE_STR(szLoginPackage);
    if (pJSON_Root)     json_decref(pJSON_Root);
    ABC_BUF_FREE(SyncKey);
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
    ABC_CHECK_NULL(pszCarePackage);

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
    ABC_CHECK_NULL(pszLoginPackage);

    json_t *pJSON_Root      = NULL;
    json_t *pJSON_MK        = NULL;
    json_t *pJSON_SyncKey   = NULL;
    json_t *pJSON_ELP2      = NULL;
    json_t *pJSON_ELRA3     = NULL;

    // Encrypt MK:
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->MK, pSelf->LP2,
        ABC_CryptoType_AES256, &pJSON_MK, pError));

    // Encrypt SyncKey:
    tABC_U08Buf SyncKey = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(SyncKey, (unsigned char *)pSelf->szSyncKey, strlen(pSelf->szSyncKey) + 1);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(SyncKey, pSelf->L4,
        ABC_CryptoType_AES256, &pJSON_SyncKey, pError));

    // Build the main body:
    pJSON_Root = json_pack("{s:o, s:o}",
        JSON_ACCT_MK_FIELD,      pJSON_MK,
        JSON_ACCT_SYNCKEY_FIELD, pJSON_SyncKey);

    // Build the recovery, if any:
    if (ABC_BUF_SIZE(pSelf->LRA3))
    {
        ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->LP2, pSelf->LRA3,
            ABC_CryptoType_AES256, &pJSON_ELP2, pError));
        ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(pSelf->LRA3, pSelf->LP2,
            ABC_CryptoType_AES256, &pJSON_ELRA3, pError));

        json_object_set(pJSON_Root, JSON_ACCT_ELP2_FIELD, pJSON_ELP2);
        json_object_set(pJSON_Root, JSON_ACCT_ELRA3_FIELD, pJSON_ELRA3);
    }

    // Write out:
    *pszLoginPackage = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    ABC_CHECK_NULL(*pszLoginPackage);

exit:
    if (pJSON_Root)     json_decref(pJSON_Root);
    if (pJSON_MK)       json_decref(pJSON_MK);
    if (pJSON_SyncKey)  json_decref(pJSON_SyncKey);
    if (pJSON_ELP2)     json_decref(pJSON_ELP2);
    if (pJSON_ELRA3)    json_decref(pJSON_ELRA3);

    return cc;
}
