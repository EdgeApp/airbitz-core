/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "AccountCategories.hpp"
#include "../crypto/Crypto.hpp"
#include "../util/FileIO.hpp"
#include "../util/Json.hpp"
#include "../util/Mutex.hpp"
#include "../util/Util.hpp"

namespace abcd {

#define ACCOUNT_CATEGORIES_FILENAME             "Categories.json"
#define JSON_ACCT_CATEGORIES_FIELD              "categories"

static tABC_CC ABC_AccountCategoriesSave(tABC_SyncKeys *pKeys, char **aszCategories, unsigned int Count, tABC_Error *pError);

/**
 * This function gets the categories for an account.
 * An array of allocated strings is allocated so the user is responsible for
 * free'ing all the elements as well as the array itself.
 */
tABC_CC ABC_AccountCategoriesLoad(tABC_SyncKeys *pKeys,
                                  char ***paszCategories,
                                  unsigned int *pCount,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szFilename = NULL;
    AutoU08Buf data;

    *paszCategories = NULL;
    *pCount = 0;
    bool bExists = false;

    // Find the file:
    ABC_STR_NEW(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pKeys->szSyncDir, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));

    // Load the entries (if any):
    if (bExists)
    {
        ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, pKeys->MK, &data, pError));
        ABC_CHECK_RET(ABC_UtilGetArrayValuesFromJSONString((char *)data.p, JSON_ACCT_CATEGORIES_FIELD, paszCategories, pCount, pError));
    }

exit:
    ABC_FREE_STR(szFilename);

    return cc;
}

/**
 * This function adds a category to an account.
 * No attempt is made to avoid a duplicate entry.
 */
tABC_CC ABC_AccountCategoriesAdd(tABC_SyncKeys *pKeys,
                                 char *szCategory,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    AutoStringArray categories;

    // load the current categories
    ABC_CHECK_RET(ABC_AccountCategoriesLoad(pKeys, &categories.data, &categories.size, pError));

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
    ABC_CHECK_RET(ABC_AccountCategoriesSave(pKeys, categories.data, categories.size, pError));

exit:
    return cc;
}

/**
 * This function removes a category from an account.
 * If there is more than one category with this name, all categories by this name are removed.
 * If the category does not exist, no error is returned.
 */
tABC_CC ABC_AccountCategoriesRemove(tABC_SyncKeys *pKeys,
                                    char *szCategory,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    AutoStringArray oldCat;
    AutoStringArray newCat;

    // load the current categories
    ABC_CHECK_RET(ABC_AccountCategoriesLoad(pKeys, &oldCat.data, &oldCat.size, pError));

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
    ABC_CHECK_RET(ABC_AccountCategoriesSave(pKeys, newCat.data, newCat.size, pError));

exit:
    return cc;
}

/**
 * Saves the categories for the given account
 */
static
tABC_CC ABC_AccountCategoriesSave(tABC_SyncKeys *pKeys,
                                  char **aszCategories,
                                  unsigned int count,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *dataJSON = NULL;
    char *szFilename = NULL;

    // create the categories JSON
    ABC_CHECK_RET(ABC_UtilCreateArrayJSONObject(aszCategories, count, JSON_ACCT_CATEGORIES_FIELD, &dataJSON, pError));

    // write them out
    ABC_STR_NEW(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pKeys->szSyncDir, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(dataJSON, pKeys->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    if (dataJSON)       json_decref(dataJSON);
    ABC_FREE_STR(szFilename);

    return cc;
}

} // namespace abcd
