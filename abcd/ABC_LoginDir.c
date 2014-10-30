/**
 * @file
 * Storage backend for login data.
 */

#include "ABC_LoginDir.h"
#include "util/ABC_FileIO.h"
#include "util/ABC_Sync.h"
#include "util/ABC_Util.h"
#include <jansson.h>

#define ACCOUNT_MAX                             1024  // maximum number of accounts
#define ACCOUNT_DIR                             "Accounts"
#define ACCOUNT_FOLDER_PREFIX                   "Account"
#define ACCOUNT_NAME_FILENAME                   "UserName.json"
#define ACCOUNT_CARE_PACKAGE_FILENAME           "CarePackage.json"
#define ACCOUNT_LOGIN_PACKAGE_FILENAME          "LoginPackage.json"
#define ACCOUNT_SYNC_DIR                        "sync"

// UserName.json:
#define JSON_ACCT_USERNAME_FIELD                "userName"

static tABC_CC ABC_LoginDirNewNumber(int *pAccountNum, tABC_Error *pError);
static tABC_CC ABC_LoginUserForNum(unsigned int AccountNum, char **pszUserName, tABC_Error *pError);
static tABC_CC ABC_LoginCreateRootDir(tABC_Error *pError);
static tABC_CC ABC_LoginCopyRootDirName(char *szRootDir, tABC_Error *pError);
static tABC_CC ABC_LoginCopyAccountDirName(char *szAccountDir, int AccountNum, tABC_Error *pError);
static tABC_CC ABC_LoginMakeFilename(char **pszOut, unsigned AccountNum, const char *szFile, tABC_Error *pError);

/**
 * Locates the account directory for a given username.
 */
tABC_CC ABC_LoginDirGetNumber(const char *szUserName,
                              int *pAccountNum,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szCurUserName = NULL;
    char *szAccountRoot = NULL;
    tABC_FileIOList *pFileList = NULL;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(pAccountNum);

    // assume we didn't find it
    *pAccountNum = -1;

    // make sure the accounts directory is in place
    ABC_CHECK_RET(ABC_LoginCreateRootDir(pError));

    // get the account root directory string
    ABC_ALLOC(szAccountRoot, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_LoginCopyRootDirName(szAccountRoot, pError));

    // get all the files in this root

    ABC_FileIOCreateFileList(&pFileList, szAccountRoot, NULL);
    for (int i = 0; i < pFileList->nCount; i++)
    {
        // if this file is a directory
        if (pFileList->apFiles[i]->type == ABC_FILEIOFileType_Directory)
        {
            // if this directory starts with the right prefix
            if ((strlen(pFileList->apFiles[i]->szName) > strlen(ACCOUNT_FOLDER_PREFIX)) &&
                (strncmp(ACCOUNT_FOLDER_PREFIX, pFileList->apFiles[i]->szName, strlen(ACCOUNT_FOLDER_PREFIX)) == 0))
            {
                char *szAccountNum = (char *)(pFileList->apFiles[i]->szName + strlen(ACCOUNT_FOLDER_PREFIX));
                unsigned int AccountNum = (unsigned int) strtol(szAccountNum, NULL, 10); // 10 is for base-10

                // get the username for this account
                tABC_Error error;
                if (ABC_CC_Ok != ABC_LoginUserForNum(AccountNum, &szCurUserName, &error))
                {
                    continue;
                }

                // if this matches what we are looking for
                if (strcmp(szUserName, szCurUserName) == 0)
                {
                    *pAccountNum = AccountNum;
                    break;
                }
                ABC_FREE_STR(szCurUserName);
                szCurUserName = NULL;
            }
        }
    }

exit:
    ABC_FREE_STR(szCurUserName);
    ABC_FREE_STR(szAccountRoot);
    ABC_FileIOFreeFileList(pFileList);

    return cc;
}


/**
 * If the login directory does not exist, create it.
 * This is meant to be called after `ABC_LoginDirGetNumber`,
 * and will do nothing if the account number is already set up.
 */
tABC_CC ABC_LoginDirCreate(int *pAccountNum,
                           const char *szUserName,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char    szAccountDir[ABC_FILEIO_MAX_PATH_LENGTH] = "";
    char    *szNameJSON = NULL;

    // We don't need to do anything if the directory already exists:
    if (0 <= *pAccountNum)
    {
        goto exit;
    }

    // Find next available account number:
    ABC_CHECK_RET(ABC_LoginDirNewNumber(pAccountNum, pError));

    // Create main account directory:
    ABC_CHECK_RET(ABC_LoginCopyAccountDirName(szAccountDir, *pAccountNum, pError));
    ABC_CHECK_RET(ABC_FileIOCreateDir(szAccountDir, pError));

    // Write user name:
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(szUserName, JSON_ACCT_USERNAME_FIELD, &szNameJSON, pError));
    ABC_CHECK_RET(ABC_LoginDirFileSave(szNameJSON, *pAccountNum, ACCOUNT_NAME_FILENAME, pError));

exit:
    ABC_FREE_STR(szNameJSON);
    return cc;
}

/**
 * Finds the next available account number (the number is just used for the directory name)
 */
static
tABC_CC ABC_LoginDirNewNumber(int *pAccountNum,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szAccountRoot = NULL;
    char *szAccountDir = NULL;

    ABC_CHECK_NULL(pAccountNum);

    // make sure the accounts directory is in place
    ABC_CHECK_RET(ABC_LoginCreateRootDir(pError));

    // get the account root directory string
    ABC_ALLOC(szAccountRoot, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_LoginCopyRootDirName(szAccountRoot, pError));

    // run through all the account names
    ABC_ALLOC(szAccountDir, ABC_FILEIO_MAX_PATH_LENGTH);
    int AccountNum;
    for (AccountNum = 0; AccountNum < ACCOUNT_MAX; AccountNum++)
    {
        ABC_CHECK_RET(ABC_LoginCopyAccountDirName(szAccountDir, AccountNum, pError));
        bool bExists = false;
        ABC_CHECK_RET(ABC_FileIOFileExists(szAccountDir, &bExists, pError));
        if (true != bExists)
        {
            break;
        }
    }

    // if we went to the end
    if (AccountNum == ACCOUNT_MAX)
    {
        ABC_RET_ERROR(ABC_CC_NoAvailAccountSpace, "No account space available");
    }

    *pAccountNum = AccountNum;

exit:
    ABC_FREE_STR(szAccountRoot);
    ABC_FREE_STR(szAccountDir);

    return cc;
}

/**
 * Gets the user name for the specified account number
 *
 * @param pszUserName Location to store allocated pointer (must be free'd by caller)
 */
static tABC_CC ABC_LoginUserForNum(unsigned int AccountNum, char **pszUserName, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *root = NULL;
    char *szJSON = NULL;

    ABC_CHECK_NULL(pszUserName);

    ABC_CHECK_RET(ABC_LoginDirFileLoad(&szJSON, AccountNum, ACCOUNT_NAME_FILENAME, pError));

    // parse out the user name
    json_error_t error;
    root = json_loads(szJSON, 0, &error);
    ABC_CHECK_ASSERT(root != NULL, ABC_CC_JSONError, "Error parsing JSON account name");
    ABC_CHECK_ASSERT(json_is_object(root), ABC_CC_JSONError, "Error parsing JSON account name");

    json_t *jsonVal = json_object_get(root, JSON_ACCT_USERNAME_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON account name");
    const char *szUserName = json_string_value(jsonVal);

    ABC_STRDUP(*pszUserName, szUserName);

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

    char *szAccountRoot = NULL;

    // create the account directory string
    ABC_ALLOC(szAccountRoot, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_LoginCopyRootDirName(szAccountRoot, pError));

    // if it doesn't exist
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szAccountRoot, &bExists, pError));
    if (true != bExists)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szAccountRoot, pError));
    }

exit:
    ABC_FREE_STR(szAccountRoot);

    return cc;
}

/**
 * Copies the root account directory into the string given
 *
 * @param szRootDir pointer into which to copy the string
 */
static
tABC_CC ABC_LoginCopyRootDirName(char *szRootDir, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szFileIORootDir = NULL;

    ABC_CHECK_NULL(szRootDir);

    ABC_CHECK_RET(ABC_FileIOGetRootDir(&szFileIORootDir, pError));

    // create the account directory string
    if (gbIsTestNet)
    {
        sprintf(szRootDir, "%s/%s-testnet", szFileIORootDir, ACCOUNT_DIR);
    }
    else
    {
        sprintf(szRootDir, "%s/%s", szFileIORootDir, ACCOUNT_DIR);
    }

exit:
    ABC_FREE_STR(szFileIORootDir);

    return cc;
}

/*
 * Copies the account directory name into the string given
 */
static
tABC_CC ABC_LoginCopyAccountDirName(char *szAccountDir, int AccountNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szAccountRoot = NULL;

    ABC_CHECK_NULL(szAccountDir);

    // get the account root directory string
    ABC_ALLOC(szAccountRoot, ABC_FILEIO_MAX_PATH_LENGTH);
    ABC_CHECK_RET(ABC_LoginCopyRootDirName(szAccountRoot, pError));

    // create the account directory string
    sprintf(szAccountDir, "%s/%s%d", szAccountRoot, ACCOUNT_FOLDER_PREFIX, AccountNum);

exit:
    ABC_FREE_STR(szAccountRoot);

    return cc;
}

/**
 * Reads a file from the account directory.
 */
tABC_CC ABC_LoginDirFileLoad(char **pszData,
                             unsigned AccountNum,
                             const char *szFile,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;

    ABC_CHECK_RET(ABC_LoginMakeFilename(&szFilename, AccountNum, szFile, pError));
    ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, pszData, pError));

exit:
    ABC_FREE_STR(szFilename);
    return cc;
}

/**
 * Writes a file to the account directory.
 */
tABC_CC ABC_LoginDirFileSave(const char *szData,
                             unsigned AccountNum,
                             const char *szFile,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;

    ABC_CHECK_RET(ABC_LoginMakeFilename(&szFilename, AccountNum, szFile, pError));
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szData, pError));

exit:
    ABC_FREE_STR(szFilename);
    return cc;
}

/**
 * Determines whether or not a file exists in the account directory.
 */
tABC_CC ABC_LoginDirFileExists(bool *pbExists,
                               unsigned AccountNum,
                               const char *szFile,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;

    ABC_CHECK_RET(ABC_LoginMakeFilename(&szFilename, AccountNum, szFile, pError));
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, pbExists, pError));

exit:
    ABC_FREE_STR(szFilename);
    return cc;
}

/**
 * Assembles a filename from its component parts.
 */
static
tABC_CC ABC_LoginMakeFilename(char **pszOut, unsigned AccountNum, const char *szFile, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char szAccountRoot[ABC_FILEIO_MAX_PATH_LENGTH];
    char szFilename[ABC_FILEIO_MAX_PATH_LENGTH];

    ABC_CHECK_ASSERT(0 <= AccountNum, ABC_CC_FileDoesNotExist, "No account directory");

    // make sure the accounts directory is in place
    ABC_CHECK_RET(ABC_LoginCreateRootDir(pError));

    // Form the filename:
    ABC_CHECK_RET(ABC_LoginCopyRootDirName(szAccountRoot, pError));
    snprintf(szFilename, sizeof(szFilename),
        "%s/%s%d/%s",
        szAccountRoot,
        ACCOUNT_FOLDER_PREFIX, AccountNum,
        szFile);

    ABC_STRDUP(*pszOut, szFilename);

exit:
    return cc;
}

/**
 * Loads the login and care packages from disk.
 */
tABC_CC ABC_LoginDirLoadPackages(int AccountNum,
                                 tABC_CarePackage **ppCarePackage,
                                 tABC_LoginPackage **ppLoginPackage,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szCarePackage = NULL;
    char *szLoginPackage = NULL;

    ABC_CHECK_RET(ABC_LoginDirFileLoad(&szCarePackage, AccountNum,
        ACCOUNT_CARE_PACKAGE_FILENAME, pError));
    ABC_CHECK_RET(ABC_LoginDirFileLoad(&szLoginPackage, AccountNum,
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
tABC_CC ABC_LoginDirSavePackages(int AccountNum,
                                 tABC_CarePackage *pCarePackage,
                                 tABC_LoginPackage *pLoginPackage,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szCarePackage = NULL;
    char *szLoginPackage = NULL;

    ABC_CHECK_RET(ABC_CarePackageEncode(pCarePackage, &szCarePackage, pError));
    ABC_CHECK_RET(ABC_LoginPackageEncode(pLoginPackage, &szLoginPackage, pError));

    ABC_CHECK_RET(ABC_LoginDirFileSave(szCarePackage, AccountNum,
        ACCOUNT_CARE_PACKAGE_FILENAME, pError));
    ABC_CHECK_RET(ABC_LoginDirFileSave(szLoginPackage, AccountNum,
        ACCOUNT_LOGIN_PACKAGE_FILENAME, pError));

exit:
    if (szCarePackage)  ABC_FREE_STR(szCarePackage);
    if (szLoginPackage) ABC_FREE_STR(szLoginPackage);
    return cc;
}

/**
 * Gets the account sync directory for a given user.
 */
tABC_CC ABC_LoginDirGetSyncDir(int AccountNum,
                               char **pszDirName,
                               tABC_Error *pError)
{
    return ABC_LoginMakeFilename(pszDirName, AccountNum, ACCOUNT_SYNC_DIR, pError);
}
