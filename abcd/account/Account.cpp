/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Account.hpp"
#include "../crypto/Crypto.hpp"
#include "../crypto/Encoding.hpp"
#include "../util/FileIO.hpp"
#include "../util/Mutex.hpp"
#include "../util/Util.hpp"

namespace abcd {

#define ACCOUNT_WALLET_DIRNAME                  "Wallets"
#define ACCOUNT_WALLET_FILENAME                 "%s/Wallets/%s.json"

// Wallet JSON fields:
#define JSON_ACCT_WALLET_MK_FIELD               "MK"
#define JSON_ACCT_WALLET_BPS_FIELD              "BitcoinSeed"
#define JSON_ACCT_WALLET_SYNC_KEY_FIELD         "SyncKey"
#define JSON_ACCT_WALLET_ARCHIVE_FIELD          "Archived"
#define JSON_ACCT_WALLET_SORT_FIELD             "SortIndex"

static tABC_CC ABC_AccountWalletGetDir(tABC_SyncKeys *pKeys, char **pszWalletDir, tABC_Error *pError);
static int ABC_AccountWalletCompare(const void *a, const void *b);

/**
 * Releases the members of a tABC_AccountWalletInfo structure. Unlike most
 * types in the ABC, this does *not* free the structure itself. This allows
 * the structure to be allocated on the stack, or as part of an array.
 */
void ABC_AccountWalletInfoFree(tABC_AccountWalletInfo *pInfo)
{
    if (pInfo)
    {
        ABC_FREE_STR(pInfo->szUUID);
        ABC_BUF_FREE(pInfo->BitcoinSeed);
        ABC_BUF_FREE(pInfo->SyncKey);
        ABC_BUF_FREE(pInfo->MK);
    }
}

/**
 * Releases an array of tABC_AccountWalletInfo structures and their
 * contained members.
 */
void ABC_AccountWalletInfoFreeArray(tABC_AccountWalletInfo *aInfo,
                                    unsigned count)
{
    if (aInfo)
    {
        for (unsigned i = 0; i < count; ++i)
        {
            ABC_AccountWalletInfoFree(aInfo + i);
        }
        ABC_CLEAR_FREE(aInfo, count*sizeof(tABC_AccountWalletInfo));
    }
}

/**
 * Returns the name of the wallet directory, creating it if necessary.
 */
static
tABC_CC ABC_AccountWalletGetDir(tABC_SyncKeys *pKeys,
                                char **pszWalletDir,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletDir = NULL;

    // Get the name:
    ABC_STR_NEW(szWalletDir, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szWalletDir, "%s/%s", pKeys->szSyncDir, ACCOUNT_WALLET_DIRNAME);

    // Create if neccessary:
    ABC_CHECK_NEW(fileEnsureDir(szWalletDir), pError);

    // Output:
    if (pszWalletDir)
    {
        *pszWalletDir = szWalletDir;
        szWalletDir = NULL;
    }

exit:
    ABC_FREE_STR(szWalletDir);

    return cc;
}

/**
 * Helper function for qsort.
 */
static int ABC_AccountWalletCompare(const void *a, const void *b)
{
    const tABC_AccountWalletInfo *pA = (const tABC_AccountWalletInfo*)a;
    const tABC_AccountWalletInfo *pB = (const tABC_AccountWalletInfo*)b;

    return pA->sortIndex - pB->sortIndex;
}

/**
 * Lists the wallets in the account. This function loads and decrypts
 * all the wallets to determine the sort order, so it is rather expensive.
 * @param aszUUID   Returned array of pointers to wallet ID's. This can
 *                  be null if the caller doesn't care. The caller frees
 *                  the result if not.
 * @param pCount    The returned number of wallets. Must not be null.
 */
tABC_CC ABC_AccountWalletList(tABC_SyncKeys *pKeys,
                              char ***paszUUID,
                              unsigned *pCount,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_AccountWalletInfo *aInfo = NULL;
    unsigned count = 0;

    ABC_CHECK_RET(ABC_AccountWalletsLoad(pKeys, &aInfo, &count, pError));

    if (paszUUID)
    {
        char **aszUUID;
        ABC_ARRAY_NEW(aszUUID, count, char*);
        for (unsigned i = 0; i < count; ++i)
        {
            aszUUID[i] = aInfo[i].szUUID;
            aInfo[i].szUUID = NULL;
        }
        *paszUUID = aszUUID;
    }
    *pCount = count;

exit:
    ABC_AccountWalletInfoFreeArray(aInfo, count);

    return cc;
}

/**
 * Loads all the wallets contained in the account.
 * @param paInfo The output list of wallets. The caller frees this.
 * @param pCount The number of returned wallets.
 */
tABC_CC ABC_AccountWalletsLoad(tABC_SyncKeys *pKeys,
                               tABC_AccountWalletInfo **paInfo,
                               unsigned *pCount,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletDir = NULL;
    tABC_FileIOList *pFileList = NULL;
    unsigned entries = 0;
    unsigned count = 0;
    tABC_AccountWalletInfo *aInfo = NULL;
    char *szUUID = NULL;

    // List the wallet directory:
    ABC_CHECK_RET(ABC_AccountWalletGetDir(pKeys, &szWalletDir, pError));
    ABC_CHECK_RET(ABC_FileIOCreateFileList(&pFileList, szWalletDir, pError));
    for (int i = 0; i < pFileList->nCount; i++)
    {
        size_t len = strlen(pFileList->apFiles[i]->szName);
        if (5 <= len &&
            !strcmp(pFileList->apFiles[i]->szName + len - 5, ".json"))
        {
            ++entries;
        }
    }

    // Load the wallets into the array:
    ABC_ARRAY_NEW(aInfo, entries, tABC_AccountWalletInfo);
    for (int i = 0; i < pFileList->nCount; ++i)
    {
        size_t len = strlen(pFileList->apFiles[i]->szName);
        if (5 <= len &&
            !strcmp(pFileList->apFiles[i]->szName + len - 5, ".json"))
        {
            char *szUUID = strndup(pFileList->apFiles[i]->szName, len - 5);
            ABC_CHECK_NULL(szUUID);

            ABC_CHECK_RET(ABC_AccountWalletLoad(pKeys, szUUID, aInfo + count, pError));
            ABC_FREE_STR(szUUID);
            ++count;
        }
    }

    // Sort the array:
    qsort(aInfo, count, sizeof(tABC_AccountWalletInfo),
          ABC_AccountWalletCompare);

    // Save the output:
    *paInfo = aInfo;
    *pCount = count;
    aInfo = NULL;

exit:
    ABC_FREE_STR(szWalletDir);
    ABC_FileIOFreeFileList(pFileList);
    ABC_AccountWalletInfoFreeArray(aInfo, count);
    ABC_FREE_STR(szUUID);

    return cc;
}

/**
 * Loads the info file for a single wallet in the account.
 * @param szUUID    The wallet to access
 * @param ppInfo    The returned wallet info. Unlike most types in ABC, the
 *                  caller must allocate this structure. This function merely
 *                  fills in the fields. This allows the structure to be a
 *                  part of an array.
 */
tABC_CC ABC_AccountWalletLoad(tABC_SyncKeys *pKeys,
                              const char *szUUID,
                              tABC_AccountWalletInfo *pInfo,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    char *szFilename = NULL;
    json_t *pJSON = NULL;
    const char *szSyncKey = NULL;
    const char *szMK = NULL;
    const char *szBPS = NULL;
    DataChunk syncKey, dataKey, bitcoinKey;

    // Load and decrypt:
    ABC_STR_NEW(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, ACCOUNT_WALLET_FILENAME, pKeys->szSyncDir, szUUID);
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, pKeys->MK, &pJSON, pError));

    // Wallet name:
    ABC_STRDUP(pInfo->szUUID, szUUID);

    // JSON-decode everything:
    e = json_unpack(pJSON, "{ss, ss, ss, si, sb}",
                    JSON_ACCT_WALLET_SYNC_KEY_FIELD, &szSyncKey,
                    JSON_ACCT_WALLET_MK_FIELD, &szMK,
                    JSON_ACCT_WALLET_BPS_FIELD, &szBPS,
                    JSON_ACCT_WALLET_SORT_FIELD, &pInfo->sortIndex,
                    JSON_ACCT_WALLET_ARCHIVE_FIELD, &pInfo->archived);
    ABC_CHECK_SYS(!e, "json_unpack(account wallet data)");

    // Decode hex strings:
    ABC_CHECK_NEW(base16Decode(syncKey, szSyncKey), pError);
    ABC_CHECK_NEW(base16Decode(dataKey, szMK), pError);
    ABC_CHECK_NEW(base16Decode(bitcoinKey, szBPS), pError);

    // Write out:
    ABC_BUF_DUP(pInfo->SyncKey, toU08Buf(syncKey));
    ABC_BUF_DUP(pInfo->MK, toU08Buf(dataKey));
    ABC_BUF_DUP(pInfo->BitcoinSeed, toU08Buf(bitcoinKey));

    // Success, so do not free the members:
    pInfo = NULL;

exit:
    ABC_AccountWalletInfoFree(pInfo);
    ABC_FREE_STR(szFilename);
    if (pJSON) json_decref(pJSON);

    return cc;
}

/**
 * Writes the info file for a single wallet in the account.
 */
tABC_CC ABC_AccountWalletSave(tABC_SyncKeys *pKeys,
                              tABC_AccountWalletInfo *pInfo,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    json_t *pJSON = NULL;
    char *szFilename = NULL;

    // JSON-encode everything:
    pJSON = json_pack("{ss, ss, ss, si, sb}",
        JSON_ACCT_WALLET_SYNC_KEY_FIELD, base16Encode(pInfo->SyncKey).c_str(),
        JSON_ACCT_WALLET_MK_FIELD, base16Encode(pInfo->MK).c_str(),
        JSON_ACCT_WALLET_BPS_FIELD, base16Encode(pInfo->BitcoinSeed).c_str(),
        JSON_ACCT_WALLET_SORT_FIELD, pInfo->sortIndex,
        JSON_ACCT_WALLET_ARCHIVE_FIELD, pInfo->archived);

    // Ensure the directory exists:
    ABC_CHECK_RET(ABC_AccountWalletGetDir(pKeys, NULL, pError));

    // Write out:
    ABC_STR_NEW(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, ACCOUNT_WALLET_FILENAME, pKeys->szSyncDir, pInfo->szUUID);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON, pKeys->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    if (pJSON) json_decref(pJSON);
    ABC_FREE_STR(szFilename);

    return cc;
}

/**
 * Sets the sort order for the wallets in the account.
 * @param aszUUID   An array containing the wallet UUID's in the desired order.
 * @param count     The number of items in the array.
 */
tABC_CC ABC_AccountWalletReorder(tABC_SyncKeys *pKeys,
                                 const char *szUUIDs,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    char *uuid, *brkt;
    unsigned int i = 0;

    AutoString uuids;
    ABC_STRDUP(uuids.get(), szUUIDs);

    for (uuid = strtok_r(uuids, "\n", &brkt);
         uuid;
         uuid = strtok_r(NULL, "\n", &brkt), i++)
    {
        AutoAccountWalletInfo info;
        ABC_CHECK_RET(ABC_AccountWalletLoad(pKeys, uuid, &info, pError));
        if (info.sortIndex != i)
        {
            info.sortIndex = i;
            ABC_CHECK_RET(ABC_AccountWalletSave(pKeys, &info, pError));
        }
    }

exit:
    return cc;
}

} // namespace abcd
