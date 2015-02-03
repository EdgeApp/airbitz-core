/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginDir.hpp"
#include "../util/FileIO.hpp"
#include "../util/Json.hpp"
#include "../util/Status.hpp"
#include "../util/Sync.hpp"
#include "../util/Util.hpp"
#include <jansson.h>

namespace abcd {

#define ACCOUNT_MAX                             1024  // maximum number of accounts
#define ACCOUNT_DIR                             "/Accounts"
#define ACCOUNT_FOLDER_PREFIX                   "Account"
#define ACCOUNT_NAME_FILENAME                   "UserName.json"
#define ACCOUNT_CARE_PACKAGE_FILENAME           "CarePackage.json"
#define ACCOUNT_LOGIN_PACKAGE_FILENAME          "LoginPackage.json"
#define ACCOUNT_SYNC_DIR                        "sync"

// UserName.json:
#define JSON_ACCT_USERNAME_FIELD                "userName"

static tABC_CC ABC_LoginDirGetUsername(const std::string &directory, char **pszUserName, tABC_Error *pError);
static tABC_CC ABC_LoginCreateRootDir(tABC_Error *pError);
static tABC_CC ABC_LoginMakeFilename(char **pszOut, const std::string &directory, const char *szFile, tABC_Error *pError);

/**
 * Finds the name of the base "Accounts" directory.
 */
static std::string
accountsDirectory()
{
    std::string root;
    AutoString szRoot;
    tABC_Error error;
    if (ABC_CC_Ok == ABC_FileIOGetRootDir(&szRoot.get(), &error))
        root = szRoot;
    else
        root = '.';

    if (gbIsTestNet)
        return std::string(root) + ACCOUNT_DIR + "-testnet";
    else
        return std::string(root) + ACCOUNT_DIR;
}

/**
 * Find the next unused account directory name.
 */
static Status
newDirName(std::string &directory)
{
    std::string accountDir = accountsDirectory();
    std::string out;

    // make sure the accounts directory is in place
    ABC_CHECK_OLD(ABC_LoginCreateRootDir(&error));

    bool exists;
    unsigned i = 0;
    do
    {
        out = accountDir + '/' + ACCOUNT_FOLDER_PREFIX + std::to_string(i++);
        ABC_CHECK_OLD(ABC_FileIOFileExists(out.c_str(), &exists, &error));
    }
    while (exists);

    directory = out;

    return Status();
}

/**
 * Locates the account directory for a given username.
 */
tABC_CC ABC_LoginDirGetName(const char *szUserName,
                            std::string &directory,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string accountsDir = accountsDirectory();
    char *szCurUserName = NULL;
    tABC_FileIOList *pFileList = NULL;

    // assume we didn't find it
    directory.clear();

    // make sure the accounts directory is in place
    ABC_CHECK_RET(ABC_LoginCreateRootDir(pError));

    // get all the files in this root

    ABC_FileIOCreateFileList(&pFileList, accountsDir.c_str(), NULL);
    for (int i = 0; i < pFileList->nCount; i++)
    {
        // if this file is a directory
        if (pFileList->apFiles[i]->type == ABC_FILEIOFileType_Directory)
        {
            // if this directory starts with the right prefix
            if ((strlen(pFileList->apFiles[i]->szName) > strlen(ACCOUNT_FOLDER_PREFIX)) &&
                (strncmp(ACCOUNT_FOLDER_PREFIX, pFileList->apFiles[i]->szName, strlen(ACCOUNT_FOLDER_PREFIX)) == 0))
            {
                auto fullDir = accountsDir + '/' + pFileList->apFiles[i]->szName;

                // get the username for this account
                tABC_Error error;
                if (ABC_CC_Ok != ABC_LoginDirGetUsername(fullDir, &szCurUserName, &error))
                {
                    continue;
                }

                // if this matches what we are looking for
                if (strcmp(szUserName, szCurUserName) == 0)
                {
                    directory = fullDir;
                    break;
                }
                ABC_FREE_STR(szCurUserName);
                szCurUserName = NULL;
            }
        }
    }

exit:
    ABC_FREE_STR(szCurUserName);
    ABC_FileIOFreeFileList(pFileList);

    return cc;
}


/**
 * If the login directory does not exist, create it.
 * This is meant to be called after `ABC_LoginDirGetNumber`,
 * and will do nothing if the account number is already set up.
 */
tABC_CC ABC_LoginDirCreate(std::string &directory,
                           const char *szUserName,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char    *szNameJSON = NULL;

    // We don't need to do anything if the directory already exists:
    if (!directory.empty())
        goto exit;

    // Find next available account number:
    ABC_CHECK_NEW(newDirName(directory), pError);

    // Create main account directory:
    ABC_CHECK_RET(ABC_FileIOCreateDir(directory.c_str(), pError));

    // Write user name:
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(szUserName, JSON_ACCT_USERNAME_FIELD, &szNameJSON, pError));
    ABC_CHECK_RET(ABC_LoginDirFileSave(szNameJSON, directory, ACCOUNT_NAME_FILENAME, pError));

exit:
    ABC_FREE_STR(szNameJSON);
    return cc;
}

/**
 * Gets the user name for the specified account number
 *
 * @param pszUserName Location to store allocated pointer (must be free'd by caller)
 */
static tABC_CC ABC_LoginDirGetUsername(const std::string &directory, char **pszUserName, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    json_error_t je;

    json_t *root = NULL;
    char *szJSON = NULL;
    json_t *jsonVal = NULL;

    ABC_CHECK_NULL(pszUserName);

    ABC_CHECK_RET(ABC_LoginDirFileLoad(&szJSON, directory, ACCOUNT_NAME_FILENAME, pError));

    // parse out the user name
    root = json_loads(szJSON, 0, &je);
    ABC_CHECK_ASSERT(root != NULL, ABC_CC_JSONError, "Error parsing JSON account name");
    ABC_CHECK_ASSERT(json_is_object(root), ABC_CC_JSONError, "Error parsing JSON account name");

    jsonVal = json_object_get(root, JSON_ACCT_USERNAME_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON account name");

    ABC_STRDUP(*pszUserName, json_string_value(jsonVal));

exit:
    if (root)               json_decref(root);
    ABC_FREE_STR(szJSON);

    return cc;
}

/**
 * creates the account directory if needed
 */
static
tABC_CC ABC_LoginCreateRootDir(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::string accountsDir = accountsDirectory();
    bool bExists = false;

    // if it doesn't exist
    ABC_CHECK_RET(ABC_FileIOFileExists(accountsDir.c_str(), &bExists, pError));
    if (true != bExists)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(accountsDir.c_str(), pError));
    }

exit:
    return cc;
}

/**
 * Reads a file from the account directory.
 */
tABC_CC ABC_LoginDirFileLoad(char **pszData,
                             const std::string &directory,
                             const char *szFile,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;

    ABC_CHECK_RET(ABC_LoginMakeFilename(&szFilename, directory, szFile, pError));
    ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, pszData, pError));

exit:
    ABC_FREE_STR(szFilename);
    return cc;
}

/**
 * Writes a file to the account directory.
 */
tABC_CC ABC_LoginDirFileSave(const char *szData,
                             const std::string &directory,
                             const char *szFile,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;

    ABC_CHECK_RET(ABC_LoginMakeFilename(&szFilename, directory, szFile, pError));
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szData, pError));

exit:
    ABC_FREE_STR(szFilename);
    return cc;
}

/**
 * Determines whether or not a file exists in the account directory.
 */
tABC_CC ABC_LoginDirFileExists(bool *pbExists,
                               const std::string &directory,
                               const char *szFile,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;

    ABC_CHECK_RET(ABC_LoginMakeFilename(&szFilename, directory, szFile, pError));
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, pbExists, pError));

exit:
    ABC_FREE_STR(szFilename);
    return cc;
}

/**
 * Determines whether or not a file exists in the account directory.
 */
tABC_CC ABC_LoginDirFileDelete(const std::string &directory,
                               const char *szFile,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;

    ABC_CHECK_RET(ABC_LoginMakeFilename(&szFilename, directory, szFile, pError));
    ABC_CHECK_RET(ABC_FileIODeleteFile(szFilename, pError));

exit:
    ABC_FREE_STR(szFilename);
    return cc;
}

/**
 * Assembles a filename from its component parts.
 */
static
tABC_CC ABC_LoginMakeFilename(char **pszOut, const std::string &directory, const char *szFile, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char szFilename[ABC_FILEIO_MAX_PATH_LENGTH];

    // make sure the accounts directory is in place
    ABC_CHECK_RET(ABC_LoginCreateRootDir(pError));

    // Form the filename:
    snprintf(szFilename, sizeof(szFilename),
        "%s/%s",
        directory.c_str(),
        szFile);

    ABC_STRDUP(*pszOut, szFilename);

exit:
    return cc;
}

/**
 * Loads the login and care packages from disk.
 */
tABC_CC ABC_LoginDirLoadPackages(const std::string &directory,
                                 tABC_CarePackage **ppCarePackage,
                                 tABC_LoginPackage **ppLoginPackage,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szCarePackage = NULL;
    char *szLoginPackage = NULL;

    ABC_CHECK_RET(ABC_LoginDirFileLoad(&szCarePackage, directory,
        ACCOUNT_CARE_PACKAGE_FILENAME, pError));
    ABC_CHECK_RET(ABC_LoginDirFileLoad(&szLoginPackage, directory,
        ACCOUNT_LOGIN_PACKAGE_FILENAME, pError));

    ABC_CHECK_RET(ABC_CarePackageDecode(ppCarePackage, szCarePackage, pError));
    ABC_CHECK_RET(ABC_LoginPackageDecode(ppLoginPackage, szLoginPackage, pError));

exit:
    if (szCarePackage)  ABC_FREE_STR(szCarePackage);
    if (szLoginPackage) ABC_FREE_STR(szLoginPackage);
    return cc;
}

/**
 * Writes the login and care packages to disk.
 */
tABC_CC ABC_LoginDirSavePackages(const std::string &directory,
                                 tABC_CarePackage *pCarePackage,
                                 tABC_LoginPackage *pLoginPackage,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szCarePackage = NULL;
    char *szLoginPackage = NULL;

    ABC_CHECK_RET(ABC_CarePackageEncode(pCarePackage, &szCarePackage, pError));
    ABC_CHECK_RET(ABC_LoginPackageEncode(pLoginPackage, &szLoginPackage, pError));

    ABC_CHECK_RET(ABC_LoginDirFileSave(szCarePackage, directory,
        ACCOUNT_CARE_PACKAGE_FILENAME, pError));
    ABC_CHECK_RET(ABC_LoginDirFileSave(szLoginPackage, directory,
        ACCOUNT_LOGIN_PACKAGE_FILENAME, pError));

exit:
    if (szCarePackage)  ABC_FREE_STR(szCarePackage);
    if (szLoginPackage) ABC_FREE_STR(szLoginPackage);
    return cc;
}

/**
 * Gets the account sync directory for a given user.
 */
tABC_CC ABC_LoginDirGetSyncDir(const std::string &directory,
                               char **pszDirName,
                               tABC_Error *pError)
{
    return ABC_LoginMakeFilename(pszDirName, directory, ACCOUNT_SYNC_DIR, pError);
}

/**
 * If the sync dir doesn't exist, create it, initialize it, and sync it.
 */
tABC_CC ABC_LoginDirMakeSyncDir(const std::string &directory,
                                char *szSyncKey,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szSyncName = NULL;
    char *szTempName = NULL;
    bool bExists = false;

    // Locate the sync dir:
    ABC_CHECK_RET(ABC_LoginDirGetSyncDir(directory, &szSyncName, pError));
    ABC_CHECK_RET(ABC_FileIOFileExists(szSyncName, &bExists, pError));

    // If it doesn't exist, create it:
    if (!bExists)
    {
        int dirty = 0;
        ABC_CHECK_RET(ABC_LoginMakeFilename(&szTempName, directory, "tmp", pError));
        ABC_CHECK_RET(ABC_FileIOCreateDir(szTempName, pError));
        ABC_CHECK_RET(ABC_SyncMakeRepo(szTempName, pError));
        ABC_CHECK_RET(ABC_SyncRepo(szTempName, szSyncKey, &dirty, pError));
        ABC_CHECK_SYS(!rename(szTempName, szSyncName), "rename");
    }

exit:
    ABC_FREE_STR(szSyncName);
    ABC_FREE_STR(szTempName);

    return cc;
}

} // namespace abcd
