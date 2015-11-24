/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginDir.hpp"
#include "../Context.hpp"
#include "../json/JsonObject.hpp"
#include "../util/FileIO.hpp"
#include <dirent.h>

namespace abcd {

struct UsernameJson:
    public JsonObject
{
    ABC_JSON_STRING(username, "userName", nullptr)
};

#define ACCOUNT_NAME_FILENAME                   "UserName.json"

/**
 * Reads the username file from an account directory.
 */
static Status
readUsername(const std::string &directory, std::string &result)
{
    UsernameJson json;
    ABC_CHECK(json.load(directory + ACCOUNT_NAME_FILENAME));
    ABC_CHECK(json.usernameOk());

    result = json.username();
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

Status
loginDirCreate(std::string &directory, const std::string &username)
{
    // Make sure the accounts directory is in place:
    ABC_CHECK(fileEnsureDir(gContext->accountsDir()));

    // We don't need to do anything if our directory already exists:
    if (!directory.empty())
        return Status();

    // Create our own directory:
    ABC_CHECK(newDirName(directory));
    ABC_CHECK(fileEnsureDir(directory));

    // Write our user name:
    UsernameJson json;
    ABC_CHECK(json.usernameSet(username));
    ABC_CHECK(json.save(directory + ACCOUNT_NAME_FILENAME));

    return Status();
}

} // namespace abcd
