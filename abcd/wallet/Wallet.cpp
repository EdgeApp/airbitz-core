/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Wallet.hpp"
#include "../Context.hpp"
#include "../account/Account.hpp"

namespace abcd {

Status
Wallet::create(std::shared_ptr<Wallet> &result, Account &account, const std::string &id)
{
    result = std::shared_ptr<Wallet>(new Wallet(account, id));
    return Status();
}

Wallet::Wallet(Account &account, const std::string &id):
    account(account),
    parent_(account.shared_from_this()),
    id_(id),
    dir_(gContext->walletsDir() + id + "/")
{}

} // namespace abcd
