/*
 *  Copyright (c) 2016, AirBitz, Inc.
 *  All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_WALLET_PATHS_HPP
#define ABCD_WALLET_PATHS_HPP

#include "util/Status.hpp"
#include <list>

namespace abcd {

/**
 * Knows how to calculate paths within a wallet directory.
 */
class WalletPaths
{
public:
    WalletPaths(const std::string &walletDir): dir_(walletDir) {}

    // Directories:
    const std::string &dir() const { return dir_; }
    std::string syncDir() const { return dir_ + "sync/"; }
    std::string addressesDir() const { return dir_ + "sync/Addresses/"; }
    std::string txsDir() const { return dir_ + "sync/Transactions/"; }

    // Files:
    std::string currencyPath() const { return dir_ + "sync/Currency.json"; }
    std::string namePath() const { return dir_ + "sync/WalletName.json"; }
    std::string watcherPath() const { return dir_ + "watcher.ser"; }

private:
    std::string dir_;
};

} // namespace abcd

#endif
