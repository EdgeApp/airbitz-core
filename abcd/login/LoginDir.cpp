/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginDir.hpp"
#include "../bitcoin/Testnet.hpp"
#include "../json/JsonObject.hpp"
#include "../util/FileIO.hpp"
#include "../util/Status.hpp"
#include "../util/Sync.hpp"
#include "../util/Util.hpp"
#include <dirent.h>
#include <jansson.h>

namespace abcd {

struct UsernameFile:
    public JsonObject
{
    ABC_JSON_STRING(Username, "userName", nullptr)
};

#define ACCOUNT_DIR                             "Accounts"
#define ACCOUNT_NAME_FILENAME                   "UserName.json"
#define ACCOUNT_CARE_PACKAGE_FILENAME           "CarePackage.json"
#define ACCOUNT_LOGIN_PACKAGE_FILENAME          "LoginPackage.json"

/**
 * Finds the name of the base "Accounts" directory.
 */
static std::string
accountsDirectory()
{
    if (isTestnet())
        return getRootDir() + ACCOUNT_DIR "-testnet/";
    else
        return getRootDir() + ACCOUNT_DIR "/";
}

/**
 * Reads the username file from an account directory.
 */
static Status
readUsername(const std::string &directory, std::string &result)
{
    UsernameFile f;
    ABC_CHECK(f.load(directory + ACCOUNT_NAME_FILENAME));
    ABC_CHECK(f.hasUsername());

    result = f.getUsername();
    return Status();
}

/**
 * Finds the next unused account directory name.
 */
static Status
newDirName(std::string &result)
{
    std::string accountsDir = accountsDirectory();
    std::string directory;

    bool exists;
    unsigned i = 0;
    do
    {
        directory = accountsDir + "Account" + std::to_string(i++) + '/';
        ABC_CHECK_OLD(ABC_FileIOFileExists(directory.c_str(), &exists, &error));
    }
    while (exists);

    result = directory;
    return Status();
}

std::list<std::string>
loginDirList()
{
    std::list<std::string> out;

    std::string accountsDir = accountsDirectory();
    DIR *dir = opendir(accountsDir.c_str());
    if (!dir)
        return out;

    struct dirent *de;
    while (nullptr != (de = readdir(dir)))
    {
        // Skip hidden files:
        if (de->d_name[0] == '.')
            continue;

        auto directory = accountsDir + de->d_name + '/';

        std::string username;
        if (readUsername(directory, username))
            out.push_back(username);
    }

    closedir(dir);
    return out;
}

std::string
loginDirFind(const std::string &username)
{
    std::string out;

    std::string accountsDir = accountsDirectory();
    DIR *dir = opendir(accountsDir.c_str());
    if (!dir)
        return out;

    struct dirent *de;
    while (nullptr != (de = readdir(dir)))
    {
        // Skip hidden files:
        if (de->d_name[0] == '.')
            continue;

        auto directory = accountsDir + de->d_name + '/';

        std::string dirUsername;
        if (readUsername(directory, dirUsername) && username == dirUsername)
        {
            out = directory;
            break;
        }
    }

    closedir(dir);
    return out;
}

/**
 * If the login directory does not exist, create it.
 * This is meant to be called after `loginDirFind`,
 * and will do nothing if the account directory is already populated.
 */
tABC_CC ABC_LoginDirCreate(std::string &directory,
                           const char *szUserName,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    UsernameFile f;

    // make sure the accounts directory is in place:
    ABC_CHECK_NEW(fileEnsureDir(accountsDirectory()), pError);

    // We don't need to do anything if our directory already exists:
    if (!directory.empty())
        goto exit;

    // Find next available account number:
    ABC_CHECK_NEW(newDirName(directory), pError);

    // Create main account directory:
    ABC_CHECK_NEW(fileEnsureDir(directory), pError);

    // Write user name:
    ABC_CHECK_NEW(f.setUsername(szUserName), pError);
    ABC_CHECK_NEW(f.save(directory + ACCOUNT_NAME_FILENAME), pError);

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

    DataChunk out;
    ABC_CHECK_NEW(fileLoad(out, directory + szFile), pError);
    ABC_STRDUP(*pszData, toString(out).c_str());

exit:
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

    ABC_CHECK_NEW(fileSave(std::string(szData) + '\n',
        directory + szFile), pError);

exit:
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

    std::string filename = directory + szFile;
    ABC_CHECK_RET(ABC_FileIODeleteFile(filename.c_str(), pError));

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

} // namespace abcd
