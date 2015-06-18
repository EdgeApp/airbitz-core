/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginDir.hpp"
#include "../Context.hpp"
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
    ABC_JSON_STRING(username, "userName", nullptr)
};

#define ACCOUNT_NAME_FILENAME                   "UserName.json"
#define ACCOUNT_CARE_PACKAGE_FILENAME           "CarePackage.json"
#define ACCOUNT_LOGIN_PACKAGE_FILENAME          "LoginPackage.json"

/**
 * Reads the username file from an account directory.
 */
static Status
readUsername(const std::string &directory, std::string &result)
{
    UsernameFile f;
    ABC_CHECK(f.load(directory + ACCOUNT_NAME_FILENAME));
    ABC_CHECK(f.usernameOk());

    result = f.username();
    return Status();
}

/**
 * Finds the next unused account directory name.
 */
static Status
newDirName(std::string &result)
{
    std::string accountsDir = gContext->accountsDir();
    std::string directory;

    unsigned i = 0;
    do
    {
        directory = accountsDir + "Account" + std::to_string(i++) + '/';
    }
    while (fileExists(directory));

    result = directory;
    return Status();
}

std::list<std::string>
loginDirList()
{
    std::list<std::string> out;

    std::string accountsDir = gContext->accountsDir();
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

    std::string accountsDir = gContext->accountsDir();
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
    ABC_CHECK_NEW(fileEnsureDir(gContext->accountsDir()));

    // We don't need to do anything if our directory already exists:
    if (!directory.empty())
        goto exit;

    // Find next available account number:
    ABC_CHECK_NEW(newDirName(directory));

    // Create main account directory:
    ABC_CHECK_NEW(fileEnsureDir(directory));

    // Write user name:
    ABC_CHECK_NEW(f.usernameSet(szUserName));
    ABC_CHECK_NEW(f.save(directory + ACCOUNT_NAME_FILENAME));

exit:
    return cc;
}

} // namespace abcd
