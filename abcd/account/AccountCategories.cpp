/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "AccountCategories.hpp"
#include "Account.hpp"
#include "../json/JsonArray.hpp"
#include "../json/JsonObject.hpp"
#include "../login/Login.hpp"

namespace abcd {

struct CategoriesJson:
    public JsonObject
{
    ABC_JSON_VALUE(categories, "categories", JsonArray);
};

static std::string
categoriesPath(const Account &account)
{
    return account.dir() + "Categories.json";
}

Status
accountCategoriesSave(const Account &account,
                      const AccountCategories &categories)
{
    JsonArray arrayJson;
    for (auto &i: categories)
    {
        ABC_CHECK(arrayJson.append(json_string(i.c_str())));
    }

    CategoriesJson json;
    ABC_CHECK(json.categoriesSet(arrayJson));
    ABC_CHECK(json.save(categoriesPath(account), account.dataKey()));

    return Status();
}

Status
accountCategoriesLoad(AccountCategories &result, const Account &account)
{
    AccountCategories out;

    CategoriesJson json;
    ABC_CHECK(json.load(categoriesPath(account), account.dataKey()));

    auto arrayJson = json.categories();
    size_t size = arrayJson.size();
    for (size_t i = 0; i < size; i++)
    {
        auto stringJson = arrayJson[i];
        if (!json_is_string(stringJson.get()))
            return ABC_ERROR(ABC_CC_JSONError, "Category is not a string");

        out.insert(json_string_value(stringJson.get()));
    }

    result = std::move(out);
    return Status();
}

Status
accountCategoriesAdd(const Account &account, const std::string &category)
{
    AccountCategories categories;
    accountCategoriesLoad(categories, account);
    categories.insert(category);
    ABC_CHECK(accountCategoriesSave(account, categories));
    return Status();
}

Status
accountCategoriesRemove(const Account &account, const std::string &category)
{
    AccountCategories categories;
    ABC_CHECK(accountCategoriesLoad(categories, account));
    categories.erase(category);
    ABC_CHECK(accountCategoriesSave(account, categories));
    return Status();
}

} // namespace abcd
