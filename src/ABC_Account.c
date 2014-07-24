/**
 * @file
 * Functions for dealing with the contents of the account sync directory.
 */

#include "ABC_Account.h"
#include "ABC_FileIO.h"
#include "ABC_Util.h"

#define ACCOUNT_CATEGORIES_FILENAME             "Categories.json"

#define JSON_ACCT_CATEGORIES_FIELD              "categories"

static tABC_CC ABC_AccountCategoriesSave(tABC_SyncKeys *pKeys, char **aszCategories, unsigned int Count, tABC_Error *pError);

/**
 * Populates a fresh account sync dir with an initial set of files.
 */
tABC_CC ABC_AccountCreate(tABC_SyncKeys *pKeys,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    // Create initial categories file with no entries:
    char *aszCategories[] = {""};
    ABC_CHECK_RET(ABC_AccountCategoriesSave(pKeys, aszCategories, 0, pError));

exit:
    return cc;
}

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
    char *szJSON = NULL;

    ABC_CHECK_NULL(paszCategories);
    *paszCategories = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // load the categories
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pKeys->szSyncDir, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_FileIOReadFileStr(szFilename, &szJSON, pError));

    // load the strings of values
    ABC_CHECK_RET(ABC_UtilGetArrayValuesFromJSONString(szJSON, JSON_ACCT_CATEGORIES_FIELD, paszCategories, pCount, pError));

exit:
    ABC_FREE_STR(szJSON);
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

    char **aszCategories = NULL;
    unsigned int categoryCount = 0;

    ABC_CHECK_NULL(szCategory);

    // load the current categories
    ABC_CHECK_RET(ABC_AccountCategoriesLoad(pKeys, &aszCategories, &categoryCount, pError));

    // if there are categories
    if ((aszCategories != NULL) && (categoryCount > 0))
    {
        aszCategories = realloc(aszCategories, sizeof(char *) * (categoryCount + 1));
    }
    else
    {
        ABC_ALLOC(aszCategories, sizeof(char *));
        categoryCount = 0;
    }
    ABC_STRDUP(aszCategories[categoryCount], szCategory);
    categoryCount++;

    // save out the categories
    ABC_CHECK_RET(ABC_AccountCategoriesSave(pKeys, aszCategories, categoryCount, pError));

exit:
    ABC_UtilFreeStringArray(aszCategories, categoryCount);

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

    char **aszCategories = NULL;
    unsigned int categoryCount = 0;
    char **aszNewCategories = NULL;
    unsigned int newCategoryCount = 0;

    ABC_CHECK_NULL(szCategory);

    // load the current categories
    ABC_CHECK_RET(ABC_AccountCategoriesLoad(pKeys, &aszCategories, &categoryCount, pError));

    // got through all the categories and only add ones that are not this one
    for (int i = 0; i < categoryCount; i++)
    {
        // if this is not the string we are looking to remove then add it to our new arrary
        if (0 != strcmp(aszCategories[i], szCategory))
        {
            // if there are categories
            if ((aszNewCategories != NULL) && (newCategoryCount > 0))
            {
                aszNewCategories = realloc(aszNewCategories, sizeof(char *) * (newCategoryCount + 1));
            }
            else
            {
                ABC_ALLOC(aszNewCategories, sizeof(char *));
                newCategoryCount = 0;
            }
            ABC_STRDUP(aszNewCategories[newCategoryCount], aszCategories[i]);
            newCategoryCount++;
        }
    }

    // save out the new categories
    ABC_CHECK_RET(ABC_AccountCategoriesSave(pKeys, aszNewCategories, newCategoryCount, pError));

exit:
    ABC_UtilFreeStringArray(aszCategories, categoryCount);
    ABC_UtilFreeStringArray(aszNewCategories, newCategoryCount);

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

    char *szDataJSON = NULL;
    char *szFilename = NULL;

    // create the categories JSON
    ABC_CHECK_RET(ABC_UtilCreateArrayJSONString(aszCategories, count, JSON_ACCT_CATEGORIES_FIELD, &szDataJSON, pError));

    // write them out
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pKeys->szSyncDir, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_FileIOWriteFileStr(szFilename, szDataJSON, pError));

exit:
    ABC_FREE_STR(szDataJSON);
    ABC_FREE_STR(szFilename);

    return cc;
}
