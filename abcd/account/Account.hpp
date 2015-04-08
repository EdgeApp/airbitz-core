/*
 * Copyright (c) 2015, AirBitz, Inc.
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
class Account
{
public:
    Account(std::shared_ptr<Login> login);

    const Login &login() const { return *login_; }
    const std::string &dir() const { return dir_; }

    /**
     * Loads the object.
     * This should be in the constructor, but those can't return errors.
     */
    Status init();

    /**
     * Syncs the account with the file server.
     * This is a blocking network operation.
     */
    Status
    sync(bool &dirty);

private:
    const std::shared_ptr<Login> login_;
    const std::string dir_;

public:
    WalletList wallets;
};

} // namespace abcd

#endif
