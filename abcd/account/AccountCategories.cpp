/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "AccountCategories.hpp"
#include "../crypto/Crypto.hpp"
#include "../login/Login.hpp"
#include "../util/FileIO.hpp"
#include "../util/Json.hpp"
#include "../util/Mutex.hpp"
#include "../util/Util.hpp"

namespace abcd {

#define ACCOUNT_CATEGORIES_FILENAME             "Categories.json"
#define JSON_ACCT_CATEGORIES_FIELD              "categories"

static tABC_CC ABC_AccountCategoriesSave(const Login &login, char **aszCategories, unsigned int Count, tABC_Error *pError);

/**
 * This function gets the categories for an account.
 * An array of allocated strings is allocated so the user is responsible for
 * free'ing all the elements as well as the array itself.
 */
tABC_CC ABC_AccountCategoriesLoad(const Login &login,
                                  char ***paszCategories,
                                  unsigned int *pCount,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    AutoU08Buf data;
    *paszCategories = NULL;
    *pCount = 0;
    bool bExists = false;
    auto filename = login.syncDir() + ACCOUNT_CATEGORIES_FILENAME;

    // Find the file:
    ABC_CHECK_RET(ABC_FileIOFileExists(filename.c_str(), &bExists, pError));

    // Load the entries (if any):
    if (bExists)
    {
        ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(filename.c_str(),
            toU08Buf(login.dataKey()), &data, pError));
        ABC_CHECK_RET(ABC_UtilGetArrayValuesFromJSONString((char *)data.p, JSON_ACCT_CATEGORIES_FIELD, paszCategories, pCount, pError));
    }

exit:
    return cc;
}

/**
 * This function adds a category to an account.
 * No attempt is made to avoid a duplicate entry.
 */
tABC_CC ABC_AccountCategoriesAdd(const Login &login,
                                 char *szCategory,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    AutoStringArray categories;

    // load the current categories
    ABC_CHECK_RET(ABC_AccountCategoriesLoad(login, &categories.data, &categories.size, pError));

    // if there are categories
    if (categories.data)
    {
        ABC_ARRAY_RESIZE(categories.data, categories.size + 1, char*);
    }
    else
    {
        ABC_ARRAY_NEW(categories.data, 1, char*);
    }
    ABC_STRDUP(categories.data[categories.size++], szCategory);

    // save out the categories
    ABC_CHECK_RET(ABC_AccountCategoriesSave(login, categories.data, categories.size, pError));

exit:
    return cc;
}

/**
 * This function removes a category from an account.
 * If there is more than one category with this name, all categories by this name are removed.
 * If the category does not exist, no error is returned.
 */
tABC_CC ABC_AccountCategoriesRemove(const Login &login,
                                    char *szCategory,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    AutoStringArray oldCat;
    AutoStringArray newCat;

    // load the current categories
    ABC_CHECK_RET(ABC_AccountCategoriesLoad(login, &oldCat.data, &oldCat.size, pError));

    // got through all the categories and only add ones that are not this one
    for (unsigned i = 0; i < oldCat.size; i++)
    {
        // if this is not the string we are looking to remove then add it to our new arrary
        if (0 != strcmp(oldCat.data[i], szCategory))
        {
            // if there are categories
            if (newCat.data)
            {
                ABC_ARRAY_RESIZE(newCat.data, newCat.size + 1, char*);
            }
            else
            {
                ABC_ARRAY_NEW(newCat.data, 1, char*);
            }
            ABC_STRDUP(newCat.data[newCat.size++], oldCat.data[i]);
        }
    }

    // save out the new categories
    ABC_CHECK_RET(ABC_AccountCategoriesSave(login, newCat.data, newCat.size, pError));

exit:
    return cc;
}

/**
 * Saves the categories for the given account
 */
static
tABC_CC ABC_AccountCategoriesSave(const Login &login,
                                  char **aszCategories,
                                  unsigned int count,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *dataJSON = NULL;
    auto filename = login.syncDir() + ACCOUNT_CATEGORIES_FILENAME;

    // create the categories JSON
    ABC_CHECK_RET(ABC_UtilCreateArrayJSONObject(aszCategories, count, JSON_ACCT_CATEGORIES_FIELD, &dataJSON, pError));

    // write them out
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(dataJSON,
        toU08Buf(login.dataKey()), ABC_CryptoType_AES256,
        filename.c_str(), pError));

exit:
    if (dataJSON)       json_decref(dataJSON);

    return cc;
}

} // namespace abcd
