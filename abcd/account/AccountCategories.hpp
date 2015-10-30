/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ACCOUNT_ACCOUNT_CATEGORIES_HPP
#define ABCD_ACCOUNT_ACCOUNT_CATEGORIES_HPP

#include "../util/Status.hpp"
#include <set>

namespace abcd {

class Account;

typedef std::set<std::string> AccountCategories;

/**
 * Loads the categories from an account.
 */
Status
accountCategoriesLoad(AccountCategories &result, const Account &account);

/**
 * Adds a category to the account.
 */
Status
accountCategoriesAdd(const Account &account, const std::string &category);

/**
 * Removes a category from the account.
 */
Status
accountCategoriesRemove(const Account &account, const std::string &category);

} // namespace abcd

#endif
