/**
 * @file
 * Functions for dealing with the contents of the account sync directory.
 *
 * This file contains all of the functions associated with account creation,
 * viewing and modification.
 *
 * See LICENSE for copy, modification, and use permissions
 *
 * @author See AUTHORS
 */

#include "ABC_Account.h"
#include "util/ABC_Crypto.h"
#include "util/ABC_FileIO.h"
#include "util/ABC_Mutex.h"
#include "util/ABC_Util.h"

#define ACCOUNT_CATEGORIES_FILENAME             "Categories.json"
#define ACCOUNT_SETTINGS_FILENAME               "Settings.json"
#define ACCOUNT_WALLET_DIRNAME                  "Wallets"
#define ACCOUNT_WALLET_FILENAME                 "%s/Wallets/%s.json"

// Settings JSON fields:
#define JSON_ACCT_CATEGORIES_FIELD              "categories"
#define JSON_ACCT_FIRST_NAME_FIELD              "firstName"
#define JSON_ACCT_LAST_NAME_FIELD               "lastName"
#define JSON_ACCT_NICKNAME_FIELD                "nickname"
#define JSON_ACCT_PIN_FIELD                     "PIN"
#define JSON_ACCT_NAME_ON_PAYMENTS_FIELD        "nameOnPayments"
#define JSON_ACCT_MINUTES_AUTO_LOGOUT_FIELD     "minutesAutoLogout"
#define JSON_ACCT_LANGUAGE_FIELD                "language"
#define JSON_ACCT_NUM_CURRENCY_FIELD            "numCurrency"
#define JSON_ACCT_EX_RATE_SOURCES_FIELD         "exchangeRateSources"
#define JSON_ACCT_EX_RATE_SOURCE_FIELD          "exchangeRateSource"
#define JSON_ACCT_BITCOIN_DENOMINATION_FIELD    "bitcoinDenomination"
#define JSON_ACCT_LABEL_FIELD                   "label"
#define JSON_ACCT_LABEL_TYPE                    "labeltype"
#define JSON_ACCT_SATOSHI_FIELD                 "satoshi"
#define JSON_ACCT_ADVANCED_FEATURES_FIELD       "advancedFeatures"

// Wallet JSON fields:
#define JSON_ACCT_WALLET_MK_FIELD               "MK"
#define JSON_ACCT_WALLET_BPS_FIELD              "BitcoinSeed"
#define JSON_ACCT_WALLET_SYNC_KEY_FIELD         "SyncKey"
#define JSON_ACCT_WALLET_ARCHIVE_FIELD          "Archived"
#define JSON_ACCT_WALLET_SORT_FIELD             "SortIndex"

static tABC_CC ABC_AccountCategoriesSave(tABC_SyncKeys *pKeys, char **aszCategories, unsigned int Count, tABC_Error *pError);
static tABC_CC ABC_AccountSettingsCreateDefault(tABC_AccountSettings **ppSettings, tABC_Error *pError);
static tABC_CC ABC_AccountWalletGetDir(tABC_SyncKeys *pKeys, char **pszWalletDir, tABC_Error *pError);
static int ABC_AccountWalletCompare(const void *a, const void *b);
static tABC_CC ABC_AccountMutexLock(tABC_Error *pError);
static tABC_CC ABC_AccountMutexUnlock(tABC_Error *pError);

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
    tABC_U08Buf data;

    ABC_CHECK_NULL(paszCategories);
    *paszCategories = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // load the categories
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pKeys->szSyncDir, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, pKeys->MK, &data, pError));

    // load the strings of values
    ABC_CHECK_RET(ABC_UtilGetArrayValuesFromJSONString((char *)data.p, JSON_ACCT_CATEGORIES_FIELD, paszCategories, pCount, pError));

exit:
    ABC_BUF_FREE(data);
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

    json_t *dataJSON = NULL;
    char *szFilename = NULL;

    // create the categories JSON
    ABC_CHECK_RET(ABC_UtilCreateArrayJSONObject(aszCategories, count, JSON_ACCT_CATEGORIES_FIELD, &dataJSON, pError));

    // write them out
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pKeys->szSyncDir, ACCOUNT_CATEGORIES_FILENAME);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(dataJSON, pKeys->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    if (dataJSON)       json_decref(dataJSON);
    ABC_FREE_STR(szFilename);

    return cc;
}

/**
 * Creates default account settings
 *
 * @param ppSettings    Location to store ptr to allocated settings (caller must free)
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_AccountSettingsCreateDefault(tABC_AccountSettings **ppSettings,
                                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    tABC_AccountSettings *pSettings = NULL;
    int i = 0;

    ABC_CHECK_NULL(ppSettings);

    ABC_ALLOC(pSettings, sizeof(tABC_AccountSettings));

    pSettings->szFirstName = NULL;
    pSettings->szLastName = NULL;
    pSettings->szNickname = NULL;
    pSettings->bNameOnPayments = false;
    pSettings->minutesAutoLogout = 60;
    ABC_STRDUP(pSettings->szLanguage, "en");
    pSettings->currencyNum = CURRENCY_NUM_USD;

    pSettings->exchangeRateSources.numSources = 5;
    ABC_ALLOC(pSettings->exchangeRateSources.aSources,
              pSettings->exchangeRateSources.numSources * sizeof(tABC_ExchangeRateSource *));

    tABC_ExchangeRateSource **aSources = pSettings->exchangeRateSources.aSources;

    // USD defaults to bitstamp
    ABC_ALLOC(aSources[i], sizeof(tABC_ExchangeRateSource));
    aSources[i]->currencyNum = CURRENCY_NUM_USD;
    ABC_STRDUP(aSources[i]->szSource, ABC_BITSTAMP);
    i++;

    // CAD defaults to coinbase
    ABC_ALLOC(aSources[i], sizeof(tABC_ExchangeRateSource));
    aSources[i]->currencyNum = CURRENCY_NUM_CAD;
    ABC_STRDUP(aSources[i]->szSource, ABC_COINBASE);
    i++;

    // EUR defaults to coinbase
    ABC_ALLOC(aSources[i], sizeof(tABC_ExchangeRateSource));
    aSources[i]->currencyNum = CURRENCY_NUM_EUR;
    ABC_STRDUP(aSources[i]->szSource, ABC_COINBASE);
    i++;

    // MXN defaults to coinbase
    ABC_ALLOC(aSources[i], sizeof(tABC_ExchangeRateSource));
    aSources[i]->currencyNum = CURRENCY_NUM_MXN;
    ABC_STRDUP(aSources[i]->szSource, ABC_COINBASE);
    i++;

    // CNY defaults to coinbase
    ABC_ALLOC(aSources[i], sizeof(tABC_ExchangeRateSource));
    aSources[i]->currencyNum = CURRENCY_NUM_CNY;
    ABC_STRDUP(aSources[i]->szSource, ABC_COINBASE);

    pSettings->bitcoinDenomination.denominationType = ABC_DENOMINATION_MBTC;
    pSettings->bitcoinDenomination.satoshi = 100000;

    // assign final settings
    *ppSettings = pSettings;
    pSettings = NULL;

exit:
    ABC_AccountSettingsFree(pSettings);

    return cc;
}

/**
 * Loads the settings for a specific account using the given key
 * If no settings file exists for the given user, defaults are created
 *
 * @param pKeys         Access to the account sync dir
 * @param ppSettings    Location to store ptr to allocated settings (caller must free)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_AccountSettingsLoad(tABC_SyncKeys *pKeys,
                                tABC_AccountSettings **ppSettings,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_AccountSettings *pSettings = NULL;
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;
    json_t *pJSON_Value = NULL;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
    ABC_CHECK_NULL(ppSettings);

    // get the settings filename
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pKeys->szSyncDir, ACCOUNT_SETTINGS_FILENAME);

    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    if (true == bExists)
    {
        // load and decrypted the file into a json object
        ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, pKeys->MK, &pJSON_Root, pError));
        //ABC_DebugLog("Loaded settings JSON:\n%s\n", json_dumps(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER));

        // allocate the new settings object
        ABC_ALLOC(pSettings, sizeof(tABC_AccountSettings));
        pSettings->szFirstName = NULL;
        pSettings->szLastName = NULL;
        pSettings->szNickname = NULL;

        // get the first name
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_FIRST_NAME_FIELD);
        if (pJSON_Value)
        {
            ABC_CHECK_ASSERT(json_is_string(pJSON_Value), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_STRDUP(pSettings->szFirstName, json_string_value(pJSON_Value));
        }

        // get the last name
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_LAST_NAME_FIELD);
        if (pJSON_Value)
        {
            ABC_CHECK_ASSERT(json_is_string(pJSON_Value), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_STRDUP(pSettings->szLastName, json_string_value(pJSON_Value));
        }

        // get the nickname
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_NICKNAME_FIELD);
        if (pJSON_Value)
        {
            ABC_CHECK_ASSERT(json_is_string(pJSON_Value), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_STRDUP(pSettings->szNickname, json_string_value(pJSON_Value));
        }

        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_PIN_FIELD);
        if (pJSON_Value)
        {
            ABC_CHECK_ASSERT(json_is_string(pJSON_Value), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_STRDUP(pSettings->szPIN, json_string_value(pJSON_Value));
        }

        // get name on payments option
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_NAME_ON_PAYMENTS_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_boolean(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON boolean value");
        pSettings->bNameOnPayments = json_is_true(pJSON_Value) ? true : false;

        // get minutes auto logout
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_MINUTES_AUTO_LOGOUT_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pSettings->minutesAutoLogout = (int) json_integer_value(pJSON_Value);

        // get language
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_LANGUAGE_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
        ABC_STRDUP(pSettings->szLanguage, json_string_value(pJSON_Value));

        // get currency num
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_NUM_CURRENCY_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pSettings->currencyNum = (int) json_integer_value(pJSON_Value);

        // get advanced features
        pJSON_Value = json_object_get(pJSON_Root, JSON_ACCT_ADVANCED_FEATURES_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_boolean(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON boolean value");
        pSettings->bAdvancedFeatures = json_is_true(pJSON_Value) ? true : false;

        // get the denomination object
        json_t *pJSON_Denom = json_object_get(pJSON_Root, JSON_ACCT_BITCOIN_DENOMINATION_FIELD);
        ABC_CHECK_ASSERT((pJSON_Denom && json_is_object(pJSON_Denom)), ABC_CC_JSONError, "Error parsing JSON object value");

        // get denomination satoshi display size (e.g., 100,000 would be milli-bit coin)
        pJSON_Value = json_object_get(pJSON_Denom, JSON_ACCT_SATOSHI_FIELD);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pSettings->bitcoinDenomination.satoshi = json_integer_value(pJSON_Value);

        // get denomination type
        pJSON_Value = json_object_get(pJSON_Denom, JSON_ACCT_LABEL_TYPE);
        ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
        pSettings->bitcoinDenomination.denominationType = json_integer_value(pJSON_Value);

        // get the exchange rates array
        json_t *pJSON_Sources = json_object_get(pJSON_Root, JSON_ACCT_EX_RATE_SOURCES_FIELD);
        ABC_CHECK_ASSERT((pJSON_Sources && json_is_array(pJSON_Sources)), ABC_CC_JSONError, "Error parsing JSON array value");

        // get the number of elements in the array
        pSettings->exchangeRateSources.numSources = (int) json_array_size(pJSON_Sources);
        if (pSettings->exchangeRateSources.numSources > 0)
        {
            ABC_ALLOC(pSettings->exchangeRateSources.aSources, pSettings->exchangeRateSources.numSources * sizeof(tABC_ExchangeRateSource *));
        }

        // run through all the sources
        for (int i = 0; i < pSettings->exchangeRateSources.numSources; i++)
        {
            tABC_ExchangeRateSource *pSource = NULL;
            ABC_ALLOC(pSource, sizeof(tABC_ExchangeRateSource));

            // get the source object
            json_t *pJSON_Source = json_array_get(pJSON_Sources, i);
            ABC_CHECK_ASSERT((pJSON_Source && json_is_object(pJSON_Source)), ABC_CC_JSONError, "Error parsing JSON array element object");

            // get the currency num
            pJSON_Value = json_object_get(pJSON_Source, JSON_ACCT_NUM_CURRENCY_FIELD);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_integer(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON integer value");
            pSource->currencyNum = (int) json_integer_value(pJSON_Value);

            // get the exchange rate source
            pJSON_Value = json_object_get(pJSON_Source, JSON_ACCT_EX_RATE_SOURCE_FIELD);
            ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_STRDUP(pSource->szSource, json_string_value(pJSON_Value));

            // assign this source to the array
            pSettings->exchangeRateSources.aSources[i] = pSource;
        }

        //
        // Create the user's "fullName" based on First, Last, Nick names
        // Should probably be pulled out into its own function
        //
        {
            size_t f, l, n;
            char *fn, *ln, *nn;

            f = ABC_STRLEN(pSettings->szFirstName);
            l = ABC_STRLEN(pSettings->szLastName);
            n = ABC_STRLEN(pSettings->szNickname);

            fn = pSettings->szFirstName;
            ln = pSettings->szLastName;
            nn = pSettings->szNickname;

            if (f || l || n)
            {
                size_t bufLength = 5 + f + l + n;
                char *fullName;

                if (ABC_STRLEN(pSettings->szFullName) < bufLength)
                {
                    ABC_REALLOC(pSettings->szFullName, bufLength);
                }
                fullName = pSettings->szFullName;

                fullName[0] = 0;

                if (f)
                {
                    sprintf(fullName, "%s", fn);
                }
                if (f && l)
                {
                    sprintf(fullName, "%s ", fullName);
                }
                if (l)
                {
                    sprintf(fullName, "%s%s", fullName, ln);
                }
                if ((f || l) && n)
                {
                    sprintf(fullName, "%s - ", fullName);
                }
                if (n)
                {
                    sprintf(fullName, "%s%s", fullName, nn);
                }
            }
        }

    }
    else
    {
        // create the defaults
        ABC_CHECK_RET(ABC_AccountSettingsCreateDefault(&pSettings, pError));
    }

    // assign final settings
    *ppSettings = pSettings;
    pSettings = NULL;

 //   ABC_DebugLog("Loading settings JSON:\n%s\n", json_dumps(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER));

exit:
    ABC_AccountSettingsFree(pSettings);
    ABC_FREE_STR(szFilename);
    if (pJSON_Root) json_decref(pJSON_Root);

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Saves the settings for a specific account using the given key
 *
 * @param pKeys         Access to the account sync dir
 * @param pSettings     Pointer to settings
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_AccountSettingsSave(tABC_SyncKeys *pKeys,
                                tABC_AccountSettings *pSettings,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    json_t *pJSON_Root = NULL;
    json_t *pJSON_Denom = NULL;
    json_t *pJSON_SourcesArray = NULL;
    json_t *pJSON_Source = NULL;
    char *szFilename = NULL;
    int retVal = 0;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));
    ABC_CHECK_NULL(pSettings);

    ABC_CHECK_NUMERIC(pSettings->szPIN, ABC_CC_NonNumericPin, "The pin must be numeric.");

    // create the json for the settings
    pJSON_Root = json_object();
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_Error, "Could not create settings JSON object");

    // set the first name
    if (pSettings->szFirstName)
    {
        retVal = json_object_set_new(pJSON_Root, JSON_ACCT_FIRST_NAME_FIELD, json_string(pSettings->szFirstName));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");
    }

    // set the last name
    if (pSettings->szLastName)
    {
        retVal = json_object_set_new(pJSON_Root, JSON_ACCT_LAST_NAME_FIELD, json_string(pSettings->szLastName));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");
    }

    // set the nickname
    if (pSettings->szNickname)
    {
        retVal = json_object_set_new(pJSON_Root, JSON_ACCT_NICKNAME_FIELD, json_string(pSettings->szNickname));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");
    }

    // set the pin
    if (pSettings->szPIN)
    {
        retVal = json_object_set_new(pJSON_Root, JSON_ACCT_PIN_FIELD, json_string(pSettings->szPIN));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");
    }

    // set name on payments option
    retVal = json_object_set_new(pJSON_Root, JSON_ACCT_NAME_ON_PAYMENTS_FIELD, json_boolean(pSettings->bNameOnPayments));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // set minutes auto logout
    retVal = json_object_set_new(pJSON_Root, JSON_ACCT_MINUTES_AUTO_LOGOUT_FIELD, json_integer(pSettings->minutesAutoLogout));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // set language
    retVal = json_object_set_new(pJSON_Root, JSON_ACCT_LANGUAGE_FIELD, json_string(pSettings->szLanguage));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // set currency num
    retVal = json_object_set_new(pJSON_Root, JSON_ACCT_NUM_CURRENCY_FIELD, json_integer(pSettings->currencyNum));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // set advanced features
    retVal = json_object_set_new(pJSON_Root, JSON_ACCT_ADVANCED_FEATURES_FIELD, json_boolean(pSettings->bAdvancedFeatures));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // create the denomination section
    pJSON_Denom = json_object();
    ABC_CHECK_ASSERT(pJSON_Denom != NULL, ABC_CC_Error, "Could not create settings JSON object");

    // set denomination satoshi display size (e.g., 100,000 would be milli-bit coin)
    retVal = json_object_set_new(pJSON_Denom, JSON_ACCT_SATOSHI_FIELD, json_integer(pSettings->bitcoinDenomination.satoshi));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // set denomination type
    retVal = json_object_set_new(pJSON_Denom, JSON_ACCT_LABEL_TYPE, json_integer(pSettings->bitcoinDenomination.denominationType));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the denomination object
    retVal = json_object_set(pJSON_Root, JSON_ACCT_BITCOIN_DENOMINATION_FIELD, pJSON_Denom);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // create the exchange sources array
    pJSON_SourcesArray = json_array();
    ABC_CHECK_ASSERT(pJSON_SourcesArray != NULL, ABC_CC_Error, "Could not create settings JSON object");

    // add the exchange sources
    for (int i = 0; i < pSettings->exchangeRateSources.numSources; i++)
    {
        tABC_ExchangeRateSource *pSource = pSettings->exchangeRateSources.aSources[i];

        // create the source object
        pJSON_Source = json_object();
        ABC_CHECK_ASSERT(pJSON_Source != NULL, ABC_CC_Error, "Could not create settings JSON object");

        // set the currency num
        retVal = json_object_set_new(pJSON_Source, JSON_ACCT_NUM_CURRENCY_FIELD, json_integer(pSource->currencyNum));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

        // set the exchange rate source
        retVal = json_object_set_new(pJSON_Source, JSON_ACCT_EX_RATE_SOURCE_FIELD, json_string(pSource->szSource));
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

        // append this object to our array
        retVal = json_array_append(pJSON_SourcesArray, pJSON_Source);
        ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

        // free the source object
        if (pJSON_Source) json_decref(pJSON_Source);
        pJSON_Source = NULL;
    }

    // add the exchange sources array object
    retVal = json_object_set(pJSON_Root, JSON_ACCT_EX_RATE_SOURCES_FIELD, pJSON_SourcesArray);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // get the settings filename
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, "%s/%s", pKeys->szSyncDir, ACCOUNT_SETTINGS_FILENAME);

    // encrypt and save json
//    ABC_DebugLog("Saving settings JSON:\n%s\n", json_dumps(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER));
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, pKeys->MK, ABC_CryptoType_AES256, szFilename, pError));


exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    if (pJSON_Denom) json_decref(pJSON_Denom);
    if (pJSON_SourcesArray) json_decref(pJSON_SourcesArray);
    if (pJSON_Source) json_decref(pJSON_Source);
    ABC_FREE_STR(szFilename);

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Frees the account settings structure, along with its contents.
 */
void ABC_AccountSettingsFree(tABC_AccountSettings *pSettings)
{
    if (pSettings)
    {
        ABC_FREE_STR(pSettings->szFirstName);
        ABC_FREE_STR(pSettings->szLastName);
        ABC_FREE_STR(pSettings->szNickname);
        ABC_FREE_STR(pSettings->szLanguage);
        ABC_FREE_STR(pSettings->szPIN);
        if (pSettings->exchangeRateSources.aSources)
        {
            for (int i = 0; i < pSettings->exchangeRateSources.numSources; i++)
            {
                ABC_FREE_STR(pSettings->exchangeRateSources.aSources[i]->szSource);
                ABC_CLEAR_FREE(pSettings->exchangeRateSources.aSources[i], sizeof(tABC_ExchangeRateSource));
            }
            ABC_CLEAR_FREE(pSettings->exchangeRateSources.aSources, sizeof(tABC_ExchangeRateSource *) * pSettings->exchangeRateSources.numSources);
        }

        ABC_CLEAR_FREE(pSettings, sizeof(tABC_AccountSettings));
    }
}

/**
 * Releases the members of a tABC_AccountWalletInfo structure. Unlike most
 * types in the ABC, this does *not* free the structure itself. This allows
 * the structure to be allocated on the stack, or as part of an array.
 */
void ABC_AccountWalletInfoFree(tABC_AccountWalletInfo *pInfo)
{
    if (pInfo)
    {
        ABC_FREE_STR(pInfo->szUUID);
        ABC_BUF_FREE(pInfo->BitcoinSeed);
        ABC_BUF_FREE(pInfo->SyncKey);
        ABC_BUF_FREE(pInfo->MK);
    }
}

/**
 * Releases an array of tABC_AccountWalletInfo structures and their
 * contained members.
 */
void ABC_AccountWalletInfoFreeArray(tABC_AccountWalletInfo *aInfo,
                                    unsigned count)
{
    if (aInfo)
    {
        for (unsigned i = 0; i < count; ++i)
        {
            ABC_AccountWalletInfoFree(aInfo + i);
        }
        ABC_CLEAR_FREE(aInfo, count*sizeof(tABC_AccountWalletInfo));
    }
}

/**
 * Returns the name of the wallet directory, creating it if necessary.
 */
static
tABC_CC ABC_AccountWalletGetDir(tABC_SyncKeys *pKeys,
                                char **pszWalletDir,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletDir = NULL;

    // Get the name:
    ABC_ALLOC(szWalletDir, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szWalletDir, "%s/%s", pKeys->szSyncDir, ACCOUNT_WALLET_DIRNAME);

    // Create if neccessary:
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szWalletDir, &bExists, pError));
    if (!bExists)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szWalletDir, pError));
    }

    // Output:
    if (pszWalletDir)
    {
        *pszWalletDir = szWalletDir;
        szWalletDir = NULL;
    }

exit:
    ABC_FREE_STR(szWalletDir);

    return cc;
}

/**
 * Helper function for qsort.
 */
static int ABC_AccountWalletCompare(const void *a, const void *b)
{
    const tABC_AccountWalletInfo *pA = a;
    const tABC_AccountWalletInfo *pB = b;

    return pA->sortIndex - pB->sortIndex;
}

/**
 * Lists the wallets in the account. This function loads and decrypts
 * all the wallets to determine the sort order, so it is rather expensive.
 * @param aszUUID   Returned array of pointers to wallet ID's. This can
 *                  be null if the caller doesn't care. The caller frees
 *                  the result if not.
 * @param pCount    The returned number of wallets. Must not be null.
 */
tABC_CC ABC_AccountWalletList(tABC_SyncKeys *pKeys,
                              char ***paszUUID,
                              unsigned *pCount,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_AccountWalletInfo *aInfo = NULL;
    unsigned count = 0;

    ABC_CHECK_RET(ABC_AccountWalletsLoad(pKeys, &aInfo, &count, pError));

    if (paszUUID)
    {
        char **aszUUID;
        ABC_ALLOC_ARRAY(aszUUID, count, char*);
        for (unsigned i = 0; i < count; ++i)
        {
            aszUUID[i] = aInfo[i].szUUID;
            aInfo[i].szUUID = NULL;
        }
        *paszUUID = aszUUID;
    }
    *pCount = count;

exit:
    ABC_AccountWalletInfoFreeArray(aInfo, count);

    return cc;
}

/**
 * Loads all the wallets contained in the account.
 * @param paInfo The output list of wallets. The caller frees this.
 * @param pCount The number of returned wallets.
 */
tABC_CC ABC_AccountWalletsLoad(tABC_SyncKeys *pKeys,
                               tABC_AccountWalletInfo **paInfo,
                               unsigned *pCount,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletDir = NULL;
    tABC_FileIOList *pFileList = NULL;
    unsigned entries = 0;
    unsigned count = 0;
    tABC_AccountWalletInfo *aInfo = NULL;
    char *szUUID = NULL;

    // List the wallet directory:
    ABC_CHECK_RET(ABC_AccountWalletGetDir(pKeys, &szWalletDir, pError));
    ABC_CHECK_RET(ABC_FileIOCreateFileList(&pFileList, szWalletDir, pError));
    for (int i = 0; i < pFileList->nCount; i++)
    {
        size_t len = strlen(pFileList->apFiles[i]->szName);
        if (5 <= len &&
            !strcmp(pFileList->apFiles[i]->szName + len - 5, ".json"))
        {
            ++entries;
        }
    }

    // Load the wallets into the array:
    ABC_ALLOC_ARRAY(aInfo, entries, tABC_AccountWalletInfo);
    for (unsigned i = 0; i < pFileList->nCount; ++i)
    {
        size_t len = strlen(pFileList->apFiles[i]->szName);
        if (5 <= len &&
            !strcmp(pFileList->apFiles[i]->szName + len - 5, ".json"))
        {
            char *szUUID = strndup(pFileList->apFiles[i]->szName, len - 5);
            ABC_CHECK_NULL(szUUID);

            ABC_CHECK_RET(ABC_AccountWalletLoad(pKeys, szUUID, aInfo + count, pError));
            ABC_FREE_STR(szUUID);
            ++count;
        }
    }

    // Sort the array:
    qsort(aInfo, count, sizeof(tABC_AccountWalletInfo),
          ABC_AccountWalletCompare);

    // Save the output:
    *paInfo = aInfo;
    *pCount = count;
    aInfo = NULL;

exit:
    ABC_FREE_STR(szWalletDir);
    ABC_FileIOFreeFileList(pFileList);
    ABC_AccountWalletInfoFreeArray(aInfo, count);
    ABC_FREE_STR(szUUID);

    return cc;
}

/**
 * Loads the info file for a single wallet in the account.
 * @param szUUID    The wallet to access
 * @param ppInfo    The returned wallet info. Unlike most types in ABC, the
 *                  caller must allocate this structure. This function merely
 *                  fills in the fields. This allows the structure to be a
 *                  part of an array.
 */
tABC_CC ABC_AccountWalletLoad(tABC_SyncKeys *pKeys,
                              const char *szUUID,
                              tABC_AccountWalletInfo *pInfo,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    int e;

    char *szFilename = NULL;
    json_t *pJSON = NULL;
    const char *szSyncKey = NULL;
    const char *szMK = NULL;
    const char *szBPS = NULL;

    // Load and decrypt:
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, ACCOUNT_WALLET_FILENAME, pKeys->szSyncDir, szUUID);
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, pKeys->MK, &pJSON, pError));

    // Wallet name:
    ABC_STRDUP(pInfo->szUUID, szUUID);

    // JSON-decode everything:
    e = json_unpack(pJSON, "{ss, ss, ss, si, sb}",
                    JSON_ACCT_WALLET_SYNC_KEY_FIELD, &szSyncKey,
                    JSON_ACCT_WALLET_MK_FIELD, &szMK,
                    JSON_ACCT_WALLET_BPS_FIELD, &szBPS,
                    JSON_ACCT_WALLET_SORT_FIELD, &pInfo->sortIndex,
                    JSON_ACCT_WALLET_ARCHIVE_FIELD, &pInfo->archived);
    ABC_CHECK_SYS(!e, "json_unpack(account wallet data)");

    // Decode hex strings:
    ABC_CHECK_RET(ABC_CryptoHexDecode(szSyncKey, &pInfo->SyncKey, pError));
    ABC_CHECK_RET(ABC_CryptoHexDecode(szMK, &pInfo->MK, pError));
    ABC_CHECK_RET(ABC_CryptoHexDecode(szBPS, &pInfo->BitcoinSeed, pError));

    // Success, so do not free the members:
    pInfo = NULL;

exit:
    ABC_AccountWalletInfoFree(pInfo);
    ABC_FREE_STR(szFilename);
    if (pJSON) json_decref(pJSON);

    return cc;
}

/**
 * Writes the info file for a single wallet in the account.
 */
tABC_CC ABC_AccountWalletSave(tABC_SyncKeys *pKeys,
                              tABC_AccountWalletInfo *pInfo,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szSyncKey = NULL;
    char *szMK = NULL;
    char *szBPS = NULL;
    json_t *pJSON = NULL;
    char *szFilename = NULL;

    ABC_CHECK_RET(ABC_AccountMutexLock(pError));

    // Hex-encode the keys:
    ABC_CHECK_RET(ABC_CryptoHexEncode(pInfo->SyncKey, &szSyncKey, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(pInfo->MK, &szMK, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(pInfo->BitcoinSeed, &szBPS, pError));

    // JSON-encode everything:
    pJSON = json_pack("{ss, ss, ss, si, sb}",
                      JSON_ACCT_WALLET_SYNC_KEY_FIELD, szSyncKey,
                      JSON_ACCT_WALLET_MK_FIELD, szMK,
                      JSON_ACCT_WALLET_BPS_FIELD, szBPS,
                      JSON_ACCT_WALLET_SORT_FIELD, pInfo->sortIndex,
                      JSON_ACCT_WALLET_ARCHIVE_FIELD, pInfo->archived);

    // Ensure the directory exists:
    ABC_CHECK_RET(ABC_AccountWalletGetDir(pKeys, NULL, pError));

    // Write out:
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(szFilename, ACCOUNT_WALLET_FILENAME, pKeys->szSyncDir, pInfo->szUUID);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON, pKeys->MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    ABC_FREE_STR(szSyncKey);
    ABC_FREE_STR(szMK);
    ABC_FREE_STR(szBPS);
    if (pJSON) json_decref(pJSON);
    ABC_FREE_STR(szFilename);

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Sets the sort order for the wallets in the account.
 * @param aszUUID   An array containing the wallet UUID's in the desired order.
 * @param count     The number of items in the array.
 */
tABC_CC ABC_AccountWalletReorder(tABC_SyncKeys *pKeys,
                                 char **aszUUID,
                                 unsigned count,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    ABC_CHECK_RET(ABC_AccountMutexLock(pError));

    tABC_AccountWalletInfo info;
    memset(&info, 0, sizeof(tABC_AccountWalletInfo));

    for (unsigned i = 0; i < count; ++i)
    {
        ABC_CHECK_RET(ABC_AccountWalletLoad(pKeys, aszUUID[i], &info, pError));
        if (info.sortIndex != i)
        {
            info.sortIndex = i;
            ABC_CHECK_RET(ABC_AccountWalletSave(pKeys, &info, pError));
        }
        ABC_AccountWalletInfoFree(&info);
    }

exit:
    ABC_AccountWalletInfoFree(&info);

    ABC_AccountMutexUnlock(NULL);
    return cc;
}

/**
 * Locks the mutex.
 */
static
tABC_CC ABC_AccountMutexLock(tABC_Error *pError)
{
    return ABC_MutexLock(pError);
}

/**
 * Unlocks the mutex
 */
static
tABC_CC ABC_AccountMutexUnlock(tABC_Error *pError)
{
    return ABC_MutexUnlock(pError);
}
