/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ACCOUNT_PATHS_HPP
#define ABCD_ACCOUNT_PATHS_HPP

#include "util/Status.hpp"

namespace abcd {

/**
 * Knows how to calculate paths within a login directory.
 */
class AccountPaths
{
public:
    AccountPaths(): ok_(false) {}
    AccountPaths(const std::string &dir): ok_(true), dir_(dir) {}

    bool ok() const { return ok_; }

    // Directories:
    const std::string &dir() const { return dir_; }
    std::string syncDir() const { return dir_ + "sync/"; }
    std::string walletsDir() const { return dir_ + "sync/Wallets/"; }

    // Files:
    std::string carePackagePath() const { return dir_ + "CarePackage.json"; }
    std::string loginPackagePath() const { return dir_ + "LoginPackage.json"; }
    std::string pinPackagePath() const { return dir_ + "PinPackage.json"; }
    std::string otpKeyPath() const { return dir_ + "OtpKey.json"; }
    std::string recovery2KeyPath() const { return dir_ + "Recovery2Key.json"; }
    std::string reposPath() const { return dir_ + "Repos.json"; }
    std::string rootKeyPath() const { return dir_ + "RootKey.json"; }

private:
    bool ok_;
    std::string dir_;
};

} // namespace abcd

#endif
