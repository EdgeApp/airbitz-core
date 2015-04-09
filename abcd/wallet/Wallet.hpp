/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_WALLET_WALLET_HPP
#define ABCD_WALLET_WALLET_HPP

#include "../util/Status.hpp"
#include <memory>

namespace abcd {

class Account;

/**
 * Manages the information stored in the top-level wallet sync directory.
 */
class Wallet:
    public std::enable_shared_from_this<Wallet>
{
public:
    Account &account;

    static Status
    create(std::shared_ptr<Wallet> &result, Account &account,
        const std::string &id);

    const std::string &id() const { return id_; }

private:
    const std::shared_ptr<Account> parent_;
    const std::string id_;

    Wallet(Account &account, const std::string &id);
};

} // namespace abcd

#endif
