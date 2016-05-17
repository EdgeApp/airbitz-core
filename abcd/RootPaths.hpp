/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ROOT_PATHS_HPP
#define ABCD_ROOT_PATHS_HPP

#include "util/Status.hpp"
#include <list>

namespace abcd {

class AccountPaths;
class WalletPaths;

/**
 * Knows how to calculate paths at the top-level of the app.
 */
class RootPaths
{
public:
    RootPaths(const std::string &rootDir, const std::string &certPath);

    // Directories:
    const std::string &rootDir() const { return dir_; }
    std::string accountsDir() const;
    std::string walletsDir() const { return dir_ + "Wallets/"; }

    /**
     * Lists the accounts on the device.
     */
    std::list<std::string>
    accountList();

    /**
     * Finds the account directory for a particular username.
     * Returns an empty string if the account does not exist on the device.
     */
    Status
    accountDir(AccountPaths &result, const std::string &username);

    /**
     * Creates a fresh directory for a new account.
     */
    Status
    accountDirNew(AccountPaths &result, const std::string &username);

    /**
     * Returns the directory name for a particular wallet.
     */
    WalletPaths
    walletDir(const std::string &id);

    // Individual files:
    const std::string &certPath() const { return certPath_; }
    std::string blockCachePath() const { return dir_ + "Blocks.json"; }
    std::string exchangeCachePath() const { return dir_ + "Exchange.json"; }
    std::string feeCachePath() const { return dir_ + "Fees.json"; }
    std::string generalPath() const { return dir_ + "Servers.json"; }
    std::string questionsPath() const { return dir_ + "Questions.json"; }
    std::string logPath() const { return dir_ + "abc.log"; }
    std::string logPrevPath() const { return dir_ + "abc-prev.log"; }

private:
    const std::string dir_;
    const std::string certPath_;
};

} // namespace abcd

#endif
