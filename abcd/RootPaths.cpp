/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "RootPaths.hpp"
#include "AccountPaths.hpp"
#include "bitcoin/Testnet.hpp"
#include "json/JsonObject.hpp"
#include "util/FileIO.hpp"
#include <dirent.h>

namespace abcd {

struct UsernameJson:
    public JsonObject
{
    ABC_JSON_STRING(username, "userName", nullptr)
};

constexpr auto usernameFilename = "UserName.json";

/**
 * Reads the username file from an account directory.
 */
static Status
readUsername(const std::string &directory, std::string &result)
{
    UsernameJson json;
    ABC_CHECK(json.load(directory + usernameFilename));
    ABC_CHECK(json.usernameOk());

    result = json.username();
    return Status();
}

RootPaths::RootPaths(const std::string &rootDir, const std::string &certPath):
    dir_(fileSlashify(rootDir)),
    certPath_(certPath)
{
}

std::string
RootPaths::accountsDir() const
{
    if (isTestnet())
        return dir_ + "Accounts-testnet/";
    else
        return dir_ + "Accounts/";
}

std::list<std::string>
RootPaths::accountList()
{
    std::list<std::string> out;

    std::string accounts = accountsDir();
    DIR *dir = opendir(accounts.c_str());
    if (!dir)
        return out;

    struct dirent *de;
    while (nullptr != (de = readdir(dir)))
    {
        // Skip hidden files:
        if (de->d_name[0] == '.')
            continue;

        auto account = accounts + de->d_name + '/';

        std::string username;
        if (readUsername(account, username))
            out.push_back(username);
    }

    closedir(dir);
    return out;
}

Status
RootPaths::accountDir(AccountPaths &result, const std::string &username)
{
    std::string out;

    std::string accounts = accountsDir();
    DIR *dir = opendir(accounts.c_str());
    if (!dir)
        return ABC_ERROR(ABC_CC_FileDoesNotExist,
                         "Cannot open accounts directory");

    struct dirent *de;
    while (nullptr != (de = readdir(dir)))
    {
        // Skip hidden files:
        if (de->d_name[0] == '.')
            continue;

        auto out = accounts + de->d_name + '/';

        std::string dirUsername;
        if (readUsername(out, dirUsername) && username == dirUsername)
        {
            result = out;
            closedir(dir);
            return Status();
        }
    }

    closedir(dir);
    return ABC_ERROR(ABC_CC_FileDoesNotExist, "No account directory");
}

Status
RootPaths::accountDirNew(AccountPaths &result, const std::string &username)
{
    std::string accounts = accountsDir();
    std::string account;

    // Find an unused name:
    unsigned i = 0;
    do
    {
        account = accounts + "Account" + std::to_string(i++) + '/';
    }
    while (fileExists(account));

    // Create the directory:
    ABC_CHECK(fileEnsureDir(accounts));
    ABC_CHECK(fileEnsureDir(account));

    // Write our user name:
    UsernameJson json;
    ABC_CHECK(json.usernameSet(username));
    ABC_CHECK(json.save(account + usernameFilename));

    result = account;
    return Status();
}

} // namespace abcd
