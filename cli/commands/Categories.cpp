/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/account/AccountCategories.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::account, CategoryList, "category-list",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    AccountCategories categories;
    ABC_CHECK(accountCategoriesLoad(categories, *session.account));
    for (const auto &category: categories)
        std::cout << category << std::endl;

    return Status();
}

COMMAND(InitLevel::account, CategoryAdd, "category-add",
        " <category>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto category = argv[0];

    ABC_CHECK(accountCategoriesAdd(*session.account, category));
    return Status();
}

COMMAND(InitLevel::account, CategoryRemove, "category-remove",
        " <category>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto category = argv[0];

    ABC_CHECK(accountCategoriesRemove(*session.account, category));
    return Status();
}
