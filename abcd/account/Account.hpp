/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ACCOUNT_ACCOUNT_HPP
#define ABCD_ACCOUNT_ACCOUNT_HPP

#include "WalletList.hpp"
#include <memory>

namespace abcd {

class Login;

/**
 * Manages the account sync directory.
 */
class Account:
    public std::enable_shared_from_this<Account>
{
public:
    Login &login;

    static Status
    create(std::shared_ptr<Account> &result, Login &login);

    const std::string &dir() const { return dir_; }
    const DataChunk &dataKey() const { return dataKey_; }

    /**
     * Syncs the account with the file server.
     * This is a blocking network operation.
     */
    Status
    sync(bool &dirty);

private:
    const std::shared_ptr<Login> parent_;
    const std::string dir_;
    const DataChunk dataKey_;
    const std::string syncKey_;

    Account(Login &login, DataSlice dataKey, const std::string &syncKey);

    Status
    load();

public:
    WalletList wallets;

    // Set to the current PIN when the settings are loaded.
    // Used to detect changes to the PIN.
    std::string pin;
};

} // namespace abcd

#endif
