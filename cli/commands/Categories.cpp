/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/account/AccountCategories.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::account, CategoryList, "category-list")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, "usage: ... categories-list <user> <pass>");

    AccountCategories categories;
    ABC_CHECK(accountCategoriesLoad(categories, *session.account));
    for (const auto &category: categories)
        std::cout << category << std::endl;

    return Status();
}

COMMAND(InitLevel::account, CategoryAdd, "category-add")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... category-add <user> <pass> <category>");
    std::string category = argv[2];

    ABC_CHECK(accountCategoriesAdd(*session.account, category));
    return Status();
}

COMMAND(InitLevel::account, CategoryRemove, "category-remove")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... category-remove <user> <pass> <category>");
    std::string category = argv[2];

    ABC_CHECK(accountCategoriesRemove(*session.account, category));
    return Status();
}
