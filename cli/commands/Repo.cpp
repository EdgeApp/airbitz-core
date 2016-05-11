/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/Context.hpp"
#include "../../abcd/util/Sync.hpp"
#include <iostream>

using namespace abcd;

static std::string
repoPath(const std::string &key)
{
    return gContext->paths.rootDir() + "repo-" + key;
}

COMMAND(InitLevel::context, RepoClone, "repo-clone",
        " <sync-key>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto key = argv[0];

    const auto path = repoPath(key);
    const auto tempPath = path + "-tmp";
    ABC_CHECK(syncEnsureRepo(path, tempPath, key));

    return Status();
}

COMMAND(InitLevel::context, RepoSync, "repo-sync",
        " <sync-key>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto key = argv[0];

    bool dirty;
    const auto path = repoPath(key);
    ABC_CHECK(syncRepo(path, key, dirty));

    if (dirty)
        std::cout << "Contents changed" << std::endl;
    else
        std::cout << "No changes" << std::endl;

    return Status();
}
