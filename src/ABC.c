/**
 * @file
 * AirBitz Core API functions.
 *
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 *
 *  @author See AUTHORS
 *  @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <pthread.h>
#include <jansson.h>
#include <math.h>
#include "ABC_Debug.h"
#include "ABC.h"
#include "ABC_Login.h"
#include "ABC_Account.h"
#include "ABC_General.h"
#include "ABC_Bridge.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Wallet.h"
#include "ABC_Crypto.h"
#include "ABC_URL.h"
#include "ABC_Mutex.h"
#include "ABC_Tx.h"
#include "ABC_Exchanges.h"
#include "ABC_Sync.h"

static bool gbInitialized = false;
static tABC_BitCoin_Event_Callback gfAsyncBitCoinEventCallback = NULL;
static void *pAsyncBitCoinCallerData = NULL;
bool gbIsTestNet = false;

static tABC_Currency gaCurrencies[] = {
    { "CAD", 124, "Canadian dollar", "Canada, Saint Pierre and Miquelon" },
    { "CNY", 156, "Chinese yuan", "China" },
    { "CUP", 192, "Cuban peso", "Cuba" },
    { "EUR", 978, "Euro", "Andorra, Austria, Belgium, Cyprus, Estonia, Finland, France, Germany, Greece, Ireland, Italy, Kosovo, Latvia, Luxembourg, Malta, Monaco, Montenegro, Netherlands, Portugal, San Marino, Slovakia, Slovenia, Spain, Vatican City; see eurozone" },
    { "GBP", 826, "Pound sterling", "United Kingdom, British Crown dependencies" },
    { "MXN", 484, "Mexican peso", "Mexico" },
    { "USD", 840, "United States dollar", "American Samoa, Barbados (as well as Barbados Dollar), Bermuda (as well as Bermudian Dollar), British Indian Ocean Territory, British Virgin Islands, Caribbean Netherlands, Ecuador, El Salvador, Guam, Haiti, Marshall Islands, Federated States of Micronesia, Northern Mariana Islands, Palau, Panama, Puerto Rico, Timor-Leste, Turks and Caicos Islands, United States, U.S. Virgin Islands, Zimbabwe" },
};

#define CURRENCY_ARRAY_COUNT ((int) (sizeof(gaCurrencies) / sizeof(gaCurrencies[0])))

/**
 * Initialize the AirBitz Core library.
 *
 * The root directory for all file storage is set in this function.
 *
 * @param szRootDir                     The root directory for all files to be saved
 * @param fAsyncBitCoinEventCallback    The function that should be called when there is an asynchronous
 *                                      BitCoin event
 * @param pData                         Pointer to data to be returned back in callback
 * @param pSeedData                     Pointer to data to seed the random number generator
 * @param seedLength                    Length of the seed data
 * @param pError                        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_Initialize(const char                   *szRootDir,
                       const char                   *szCaCertPath,
                       tABC_BitCoin_Event_Callback  fAsyncBitCoinEventCallback,
                       void                         *pData,
                       const unsigned char          *pSeedData,
                       unsigned int                 seedLength,
                       tABC_Error                   *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf Seed = ABC_BUF_NULL;

    ABC_CHECK_NULL(szRootDir);
    ABC_CHECK_NULL(pSeedData);
    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "The core library has already been initalized");

    // override the alloc and free of janson so we can have a secure method
    json_set_alloc_funcs(ABC_UtilJanssonSecureMalloc, ABC_UtilJanssonSecureFree);

    // Set the callback and caller data
    gfAsyncBitCoinEventCallback = fAsyncBitCoinEventCallback;
    pAsyncBitCoinCallerData = pData;

    // initialize the mutex system
    ABC_CHECK_RET(ABC_MutexInitialize(pError));

    // initialize URL system
    ABC_CHECK_RET(ABC_URLInitialize(szCaCertPath, pError));

    // initialize the FileIO system
    ABC_CHECK_RET(ABC_FileIOInitialize(pError));

    // initialize Bitcoin transaction system
    ABC_CHECK_RET(ABC_TxInitialize(fAsyncBitCoinEventCallback, pData, pError));

    // initialize Bitcoin exchange system
    ABC_CHECK_RET(ABC_ExchangeInitialize(fAsyncBitCoinEventCallback, pData, pError));

    // initialize Crypto perf checks to determine hashing power
    ABC_CHECK_RET(ABC_InitializeCrypto(pError));


    // initialize sync
    ABC_CHECK_RET(ABC_SyncInit(szCaCertPath, pError));

    if (szRootDir)
    {
        ABC_CHECK_RET(ABC_FileIOSetRootDir(szRootDir, pError));
    }

    ABC_BUF_DUP_PTR(Seed, pSeedData, seedLength);
    ABC_CHECK_RET(ABC_CryptoSetRandomSeed(Seed, pError));

    gbInitialized = true;
    gbIsTestNet = ABC_BridgeIsTestNet();

exit:
    ABC_BUF_FREE(Seed);

    return cc;
}

/**
 * Mark the end of use of the AirBitz Core library.
 *
 * This function is the counter to ABC_Initialize.
 * It should be called when all use of the library is complete.
 *
 */
void ABC_Terminate()
{
    if (gbInitialized == true)
    {
        ABC_ClearKeyCache(NULL);

        ABC_URLTerminate();

        ABC_FileIOTerminate();

        ABC_ExchangeTerminate();

        ABC_MutexTerminate();

        ABC_SyncTerminate();

        gbInitialized = false;
    }
}

/**
 * Create a new account.
 *
 * This function kicks off a thread to signin to an account. The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param fRequestCallback          The function that will be called when the account signin process has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SignIn(const char *szUserName,
                   const char *szPassword,
                   tABC_Request_Callback fRequestCallback,
                   void *pData,
                   tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_LoginRequestInfo *pAccountRequestInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");

    ABC_CHECK_RET(ABC_LoginRequestInfoAlloc(&pAccountRequestInfo,
                                              ABC_RequestType_AccountSignIn,
                                              szUserName,
                                              szPassword,
                                              NULL, // recovery questions
                                              NULL, // recovery answers
                                              NULL, // PIN
                                              NULL, // new password
                                              fRequestCallback,
                                              pData,
                                              pError));

    if (fRequestCallback)
    {
        pthread_t handle;
        if (!pthread_create(&handle, NULL, ABC_LoginRequestThreaded, pAccountRequestInfo))
        {
            pthread_detach(handle);
        }
    }
    else
    {
        cc = ABC_LoginSignIn(pAccountRequestInfo, pError);
        ABC_LoginRequestInfoFree(pAccountRequestInfo);
    }

exit:

    return cc;
}

/**
 * Create a new account.
 *
 * This function kicks off a thread to create a new account. The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param szPIN                     PIN for the account
 * @param fRequestCallback          The function that will be called when the account create process has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CreateAccount(const char *szUserName,
                          const char *szPassword,
                          const char *szPIN,
                          tABC_Request_Callback fRequestCallback,
                          void *pData,
                          tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_LoginRequestInfo *pAccountRequestInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) >= ABC_MIN_USERNAME_LENGTH, ABC_CC_Error, "Username too short");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szPIN);
    ABC_CHECK_ASSERT(strlen(szPIN) >= ABC_MIN_PIN_LENGTH, ABC_CC_Error, "PIN is too short");

    ABC_CHECK_RET(ABC_LoginRequestInfoAlloc(&pAccountRequestInfo,
                                              ABC_RequestType_CreateAccount,
                                              szUserName,
                                              szPassword,
                                              NULL, // recovery questions
                                              NULL, // recovery answers
                                              szPIN,
                                              NULL, // new password
                                              fRequestCallback,
                                              pData,
                                              pError));

    if (fRequestCallback)
    {
        pthread_t handle;
        if (!pthread_create(&handle, NULL, ABC_LoginRequestThreaded, pAccountRequestInfo))
        {
            pthread_detach(handle);
        }
    }
    else
    {
        cc = ABC_LoginCreate(pAccountRequestInfo, pError);
        ABC_LoginRequestInfoFree(pAccountRequestInfo);
    }

exit:

    return cc;
}

/**
 * Set the recovery questions for an account
 *
 * This function kicks off a thread to set the recovery questions for an account.
 * The callback will be called when it has finished.
 *
 * @param szUserName                UserName of the account
 * @param szPassword                Password of the account
 * @param szRecoveryQuestions       Recovery questions - newline seperated
 * @param szRecoveryAnswers         Recovery answers - newline seperated
 * @param fRequestCallback          The function that will be called when the recovery questions are ready.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetAccountRecoveryQuestions(const char *szUserName,
                                        const char *szPassword,
                                        const char *szRecoveryQuestions,
                                        const char *szRecoveryAnswers,
                                        tABC_Request_Callback fRequestCallback,
                                        void *pData,
                                        tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_LoginRequestInfo *pInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szRecoveryQuestions);
    ABC_CHECK_ASSERT(strlen(szRecoveryQuestions) > 0, ABC_CC_Error, "No recovery questions provided");
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_ASSERT(strlen(szRecoveryAnswers) > 0, ABC_CC_Error, "No recovery answers provided");

    ABC_CHECK_RET(ABC_LoginRequestInfoAlloc(&pInfo,
                                              ABC_RequestType_SetAccountRecoveryQuestions,
                                              szUserName,
                                              szPassword,
                                              szRecoveryQuestions,
                                              szRecoveryAnswers,
                                              NULL, // PIN
                                              NULL, // new password
                                              fRequestCallback,
                                              pData,
                                              pError));

    if (fRequestCallback)
    {
        pthread_t handle;
        if (!pthread_create(&handle, NULL, ABC_LoginRequestThreaded, pInfo))
        {
            pthread_detach(handle);
        }
    }
    else
    {
        cc = ABC_LoginSetRecovery(pInfo, pError);
        ABC_LoginRequestInfoFree(pInfo);
    }

exit:

    return cc;
}


/**
 * Create a new wallet.
 *
 * This function kicks off a thread to create a new wallet. The callback will be called when it has finished.
 * The UUID of the new wallet will be provided in the callback pRetData as a (char *).
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param szWalletName              Wallet Name
 * @param currencyNum               ISO 4217 currency number
 * @param attributes                Attributes to be used for filtering (e.g., archive bit)
 * @param fRequestCallback          The function that will be called when the wallet create process has finished.
 * @param pData                     Pointer to data to be returned back in callback,
 *                                  or `char **pszUUID` if callbacks aren't used.
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CreateWallet(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletName,
                         int        currencyNum,
                         unsigned int attributes,
                         tABC_Request_Callback fRequestCallback,
                         void *pData,
                         tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_WalletCreateInfo *pWalletCreateInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletName);
    ABC_CHECK_ASSERT(strlen(szWalletName) > 0, ABC_CC_Error, "No wallet name provided");

    ABC_CHECK_RET(ABC_WalletCreateInfoAlloc(&pWalletCreateInfo,
                                            szUserName,
                                            szPassword,
                                            szWalletName,
                                            currencyNum,
                                            attributes,
                                            fRequestCallback,
                                            pData,
                                            pError));
    if (fRequestCallback)
    {
        pthread_t handle;
        if (!pthread_create(&handle, NULL, ABC_WalletCreateThreaded, pWalletCreateInfo))
        {
            pthread_detach(handle);
        }
    }
    else
    {
        tABC_RequestResults *results = pData;
        char * output = NULL;
        ABC_ALLOC(output, 100*sizeof(char));
        results->pRetData = output;
        cc = ABC_WalletCreate(pWalletCreateInfo, (char**) &(results->pRetData), pError);
        ABC_WalletCreateInfoFree(pWalletCreateInfo);
    }

exit:

    return cc;
}


/**
 * Clear cached keys.
 *
 * This function clears any keys that might be cached.
 *
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ClearKeyCache(tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_LoginClearKeyCache(pError));

    ABC_CHECK_RET(ABC_WalletClearCache(pError));

exit:

    return cc;
}

/**
 * Create a new wallet.
 *
 * This function provides the array of currencies.
 * The array returned should not be modified and should not be deallocated.
 *
 * @param paCurrencyArray           Pointer in which to store the currency array
 * @param pCount                    Pointer in which to store the count of array entries
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetCurrencies(tABC_Currency **paCurrencyArray,
                          int *pCount,
                          tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(paCurrencyArray);
    ABC_CHECK_NULL(pCount);

    *paCurrencyArray = gaCurrencies;
    *pCount = CURRENCY_ARRAY_COUNT;

exit:

    return cc;
}

/**
 * Get a PIN number (Deprecated!).
 *
 * This function retrieves the PIN for a given account.
 * The string is allocated and must be free'd by the caller.
 * It is deprecated in favor of just reading the PIN out of the account settings.
 *
 * @param szUserName             UserName for the account
 * @param szPassword             Password for the account
 * @param pszPIN                 Pointer where to store allocated PIN string
 * @param pError                 A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetPIN(const char *szUserName,
                   const char *szPassword,
                   char **pszPIN,
                   tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_SyncKeys *pKeys = NULL;
    tABC_AccountSettings *pSettings = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(pszPIN);

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountSettingsLoad(pKeys, &pSettings, pError));

    ABC_STRDUP(*pszPIN, pSettings->szPIN);

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);
    if (pSettings)      ABC_AccountSettingsFree(pSettings);

    return cc;
}

/**
 * Set PIN number for an account (Deprecated!).
 *
 * This function sets the PIN for a given account.
 * It is deprecated in favor of just setting the PIN in the account settings.
 *
 * @param szUserName            UserName for the account
 * @param szPassword            Password for the account
 * @param szPIN                 PIN string
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetPIN(const char *szUserName,
                   const char *szPassword,
                   const char *szPIN,
                   tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_SyncKeys *pKeys = NULL;
    tABC_AccountSettings *pSettings = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szPIN);
    ABC_CHECK_ASSERT(strlen(szPIN) >= ABC_MIN_PIN_LENGTH, ABC_CC_Error, "Pin is too short");

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountSettingsLoad(pKeys, &pSettings, pError));
    ABC_FREE_STR(pSettings->szPIN);
    ABC_STRDUP(pSettings->szPIN, szPIN);
    ABC_CHECK_RET(ABC_AccountSettingsSave(pKeys, pSettings, pError));

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);
    if (pSettings)      ABC_AccountSettingsFree(pSettings);

    return cc;
}

/**
 * Get the categories for an account.
 *
 * This function gets the categories for an account.
 * An array of allocated strings is allocated so the user is responsible for
 * free'ing all the elements as well as the array itself.
 *
 * @param szUserName            UserName for the account
 * @param paszCategories        Pointer to store results (NULL is stored if no categories)
 * @param pCount                Pointer to store result count (can be 0)
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetCategories(const char *szUserName,
                          const char *szPassword,
                          char ***paszCategories,
                          unsigned int *pCount,
                          tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_SyncKeys *pKeys = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountCategoriesLoad(pKeys, paszCategories, pCount, pError));

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);

    return cc;
}

/**
 * Add a category for an account.
 *
 * This function adds a category to an account.
 * No attempt is made to avoid a duplicate entry.
 *
 * @param szUserName            UserName for the account
 * @param szCategory            Category to add
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_AddCategory(const char *szUserName,
                        const char *szPassword,
                        char *szCategory,
                        tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_SyncKeys *pKeys = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountCategoriesAdd(pKeys, szCategory, pError));

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);

    return cc;
}


/**
 * Remove a category from an account.
 *
 * This function removes a category from an account.
 * If there is more than one category with this name, all categories by this name are removed.
 * If the category does not exist, no error is returned.
 *
 * @param szUserName            UserName for the account
 * @param szCategory            Category to remove
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_RemoveCategory(const char *szUserName,
                           const char *szPassword,
                           char *szCategory,
                           tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_SyncKeys *pKeys = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountCategoriesRemove(pKeys, szCategory, pError));

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);

    return cc;
}

/**
 * Renames a wallet.
 *
 * This function renames the wallet of a given UUID.
 * If there is more than one category with this name, all categories by this name are removed.
 * If the category does not exist, no error is returned.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param szUUID                UUID of the wallet
 * @param szNewWalletName       New name for the wallet
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_RenameWallet(const char *szUserName,
                         const char *szPassword,
                         const char *szUUID,
                         const char *szNewWalletName,
                         tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_WalletSetName(szUserName, szPassword, szUUID, szNewWalletName, pError));

exit:

    return cc;
}

/**
 * Sets (or unsets) the archive bit on a wallet.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param szUUID                UUID of the wallet
 * @param archived              True if the archive bit should be set
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetWalletArchived(const char *szUserName,
                              const char *szPassword,
                              const char *szUUID,
                              unsigned int archived,
                              tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_SyncKeys *pKeys = NULL;
    tABC_AccountWalletInfo info;
    memset(&info, 0, sizeof(tABC_AccountWalletInfo));

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountWalletLoad(pKeys, szUUID, &info, pError));
    info.archived = !!archived;
    ABC_CHECK_RET(ABC_AccountWalletSave(pKeys, &info, pError));

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);
    ABC_AccountWalletInfoFree(&info);

    return cc;
}

/**
 * Checks the validity of the given account answers.
 *
 * This function sets the attributes on a given wallet to those given.
 * The attributes would have already been set when the wallet was created so this would allow them
 * to be changed.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szRecoveryAnswers     Recovery answers - newline seperated
 * @param pbValid               Pointer to boolean into which to store result
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CheckRecoveryAnswers(const char *szUserName,
                                 const char *szRecoveryAnswers,
                                 bool *pbValid,
                                 tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_LoginCheckRecoveryAnswers(szUserName, szRecoveryAnswers, pbValid, pError));

exit:

    return cc;
}

/**
 * Gets information on the given wallet.
 *
 * This function allocates and fills in a wallet info structure with the information
 * associated with the given wallet UUID
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param szUUID                UUID of the wallet
 * @param ppWalletInfo          Pointer to store the pointer of the allocated wallet info struct
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetWalletInfo(const char *szUserName,
                          const char *szPassword,
                          const char *szUUID,
                          tABC_WalletInfo **ppWalletInfo,
                          tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_WalletGetInfo(szUserName, szPassword, szUUID, ppWalletInfo, pError));

exit:

    return cc;
}

/**
 * Free the wallet info.
 *
 * This function frees the wallet info struct returned from ABC_GetWalletInfo.
 *
 * @param pWalletInfo   Wallet info to be free'd
 */
void ABC_FreeWalletInfo(tABC_WalletInfo *pWalletInfo)

{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_WalletFreeInfo(pWalletInfo);
}

/**
 * Export the private seed used to generate all addresses within a wallet.
 * For now, this uses a simple hex dump of the raw data.
 */
tABC_CC ABC_ExportWalletSeed(const char *szUserName,
                             const char *szPassword,
                             const char *szUUID,
                             char **pszWalletSeed,
                             tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf seed = ABC_BUF_NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_WalletGetBitcoinPrivateSeed(szUserName, szPassword, szUUID, &seed, pError));
    ABC_CHECK_RET(ABC_CryptoHexEncode(seed, pszWalletSeed, pError));

exit:
    ABC_BUF_FREE(seed);

    return cc;
}

/**
 * Gets wallet UUIDs for a specified account.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param paWalletUUID          Pointer to store the allocated array of wallet info structs
 * @param pCount                Pointer to store number of wallets in the array
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetWalletUUIDs(const char *szUserName,
                           const char *szPassword,
                           char ***paWalletUUID,
                           unsigned int *pCount,
                           tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_SyncKeys *pKeys = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountWalletList(pKeys, paWalletUUID, pCount, pError));

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);

    return cc;
}

/**
 * Gets wallets for a specified account.
 *
 * This function allocates and fills in an array of wallet info structures with the information
 * associated with the wallets of the given user
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param paWalletInfo          Pointer to store the allocated array of wallet info structs
 * @param pCount                Pointer to store number of wallets in the array
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetWallets(const char *szUserName,
                       const char *szPassword,
                       tABC_WalletInfo ***paWalletInfo,
                       unsigned int *pCount,
                       tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_WalletGetWallets(szUserName, szPassword, paWalletInfo, pCount, pError));

exit:

    return cc;
}

/**
 * Free the wallet info array.
 *
 * This function frees the wallet info array returned from ABC_GetWallets.
 *
 * @param aWalletInfo   Wallet info array to be free'd
 * @param nCount        Number of elements in the array
 */
void ABC_FreeWalletInfoArray(tABC_WalletInfo **aWalletInfo,
                             unsigned int nCount)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_WalletFreeInfoArray(aWalletInfo, nCount);
}

/**
 * Set the wallet order for a specified account.
 *
 * This function sets the order of the wallets for an account to the order in the given
 * array.
 *
 * @param szUserName            UserName for the account associated with this wallet
 * @param szPassword            Password for the account associated with this wallet
 * @param aszUUIDArray          Array of UUID strings
 * @param countUUIDs            Number of UUID's in aszUUIDArray
 * @param pError                A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetWalletOrder(const char *szUserName,
                           const char *szPassword,
                           char **aszUUIDArray,
                           unsigned int countUUIDs,
                           tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_SyncKeys *pKeys = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountWalletReorder(pKeys, aszUUIDArray, countUUIDs, pError));

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);

    return cc;
}

/**
 * Get the recovery question choices.
 *
 * This is a blocking function that hits the server for the possible recovery
 * questions.
 *
 * @param pOut                      The returned question choices.
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetQuestionChoices(tABC_QuestionChoices **pOut,
                               tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_GeneralGetQuestionChoices(pOut, pError));

exit:

    return cc;
}

/**
 * Free question choices.
 *
 * This function frees the question choices given
 *
 * @param pQuestionChoices  Pointer to question choices to free.
 */
void ABC_FreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_GeneralFreeQuestionChoices(pQuestionChoices);
}

/**
 * Get the recovery questions for a given account.
 *
 * The questions will be returned in a single allocated string with
 * each questions seperated by a newline.
 *
 * @param szUserName                UserName for the account
 * @param pszQuestions              Pointer into which allocated string should be stored.
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetRecoveryQuestions(const char *szUserName,
                                 char **pszQuestions,
                                 tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(pszQuestions);

    ABC_CHECK_RET(ABC_LoginGetRecoveryQuestions(szUserName, pszQuestions, pError));

exit:

    return cc;
}

/**
 * Change account password.
 *
 * This function kicks off a thread to change the password for an account.
 * The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param szNewPassword             New Password for the account
 * @param szNewPIN                  New PIN for the account
 * @param fRequestCallback          The function that will be called when the password change has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ChangePassword(const char *szUserName,
                           const char *szPassword,
                           const char *szNewPassword,
                           const char *szNewPIN,
                           tABC_Request_Callback fRequestCallback,
                           void *pData,
                           tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_LoginRequestInfo *pAccountRequestInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szNewPassword);
    ABC_CHECK_ASSERT(strlen(szNewPassword) > 0, ABC_CC_Error, "No new password provided");
    ABC_CHECK_NULL(szNewPIN);
    ABC_CHECK_ASSERT(strlen(szNewPIN) > 0, ABC_CC_Error, "No new PIN provided");

    ABC_CHECK_RET(ABC_LoginRequestInfoAlloc(&pAccountRequestInfo,
                                              ABC_RequestType_ChangePassword,
                                              szUserName,
                                              szPassword,
                                              NULL, // recovery questions
                                              NULL, // recovery answers
                                              szNewPIN,
                                              szNewPassword,
                                              fRequestCallback,
                                              pData,
                                              pError));

    if (fRequestCallback)
    {
        pthread_t handle;
        if (!pthread_create(&handle, NULL, ABC_LoginRequestThreaded, pAccountRequestInfo))
        {
            pthread_detach(handle);
        }
    }
    else
    {
        cc = ABC_LoginChangePassword(pAccountRequestInfo, pError);
        ABC_LoginRequestInfoFree(pAccountRequestInfo);
    }

exit:

    return cc;
}

/**
 * Change account password using recovery answers.
 *
 * This function kicks off a thread to change the password for an account using the
 * recovery answers as account validation.
 * The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szRecoveryAnswers         Recovery answers (each answer seperated by a newline)
 * @param szNewPassword             New Password for the account
 * @param szNewPIN                  New PIN for the account
 * @param fRequestCallback          The function that will be called when the password change has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ChangePasswordWithRecoveryAnswers(const char *szUserName,
                                              const char *szRecoveryAnswers,
                                              const char *szNewPassword,
                                              const char *szNewPIN,
                                              tABC_Request_Callback fRequestCallback,
                                              void *pData,
                                              tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_LoginRequestInfo *pAccountRequestInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szRecoveryAnswers);
    ABC_CHECK_ASSERT(strlen(szRecoveryAnswers) > 0, ABC_CC_Error, "No recovery answers provided");
    ABC_CHECK_NULL(szNewPassword);
    ABC_CHECK_ASSERT(strlen(szNewPassword) > 0, ABC_CC_Error, "No new password provided");
    ABC_CHECK_NULL(szNewPIN);
    ABC_CHECK_ASSERT(strlen(szNewPIN) > 0, ABC_CC_Error, "No new PIN provided");

    ABC_CHECK_RET(ABC_LoginRequestInfoAlloc(&pAccountRequestInfo,
                                              ABC_RequestType_ChangePassword,
                                              szUserName,
                                              NULL, // recovery questions
                                              NULL, // password
                                              szRecoveryAnswers,
                                              szNewPIN,
                                              szNewPassword,
                                              fRequestCallback,
                                              pData,
                                              pError));

    if (fRequestCallback)
    {
        pthread_t handle;
        if (!pthread_create(&handle, NULL, ABC_LoginRequestThreaded, pAccountRequestInfo))
        {
            pthread_detach(handle);
        }
    }
    else
    {
        cc = ABC_LoginChangePassword(pAccountRequestInfo, pError);
        ABC_LoginRequestInfoFree(pAccountRequestInfo);
    }

exit:

    return cc;
}

/**
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param szURI     URI to parse
 * @param ppInfo    Pointer to location to store allocated info struct.
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ParseBitcoinURI(const char *szURI,
                            tABC_BitcoinURIInfo **ppInfo,
                            tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_BridgeParseBitcoinURI(szURI, ppInfo, pError));

exit:

    return cc;
}

/**
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param pInfo Pointer to allocated info struct.
 */
void ABC_FreeURIInfo(tABC_BitcoinURIInfo *pInfo)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_BridgeFreeURIInfo(pInfo);
}

/**
 * Converts amount from Satoshi to Bitcoin
 *
 * @param satoshi Amount in Satoshi
 */
double ABC_SatoshiToBitcoin(int64_t satoshi)
{
    return(ABC_TxSatoshiToBitcoin(satoshi));
}

/**
 * Converts amount from Bitcoin to Satoshi
 *
 * @param bitcoin Amount in Bitcoin
 */
int64_t ABC_BitcoinToSatoshi(double bitcoin)
{
    return(ABC_TxBitcoinToSatoshi(bitcoin));
}

/**
 * Converts Satoshi to given currency
 *
 * @param satoshi     Amount in Satoshi
 * @param pCurrency   Pointer to location to store amount converted to currency.
 * @param currencyNum Currency ISO 4217 num
 * @param pError      A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SatoshiToCurrency(const char *szUserName,
                              const char *szPassword,
                              int64_t satoshi,
                              double *pCurrency,
                              int currencyNum,
                              tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxSatoshiToCurrency(szUserName, szPassword, satoshi, pCurrency, currencyNum, pError));

exit:

    return cc;
}

/**
 * Converts given currency to Satoshi
 *
 * @param currency    Amount in given currency
 * @param currencyNum Currency ISO 4217 num
 * @param pSatoshi    Pointer to location to store amount converted to Satoshi
 * @param pError      A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CurrencyToSatoshi(const char *szUserName,
                              const char *szPassword,
                              double currency,
                              int currencyNum,
                              int64_t *pSatoshi,
                              tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxCurrencyToSatoshi(szUserName, szPassword, currency, currencyNum, pSatoshi, pError));

exit:

    return cc;
}


/**
 * Parses a Bitcoin amount string to an integer.
 * @param the amount to parse, in bitcoins
 * @param the integer value, in satoshis, or ABC_INVALID_AMOUNT
 * if something goes wrong.
 * @param decimalPlaces set to ABC_BITCOIN_DECIMAL_PLACE to convert
 * bitcoin to satoshis.
 */
tABC_CC ABC_ParseAmount(const char *szAmount,
                        uint64_t *pAmountOut,
                        unsigned decimalPlaces)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_BridgeParseAmount(szAmount, pAmountOut, decimalPlaces));

exit:
    return cc;
}

/**
 * Formats a Bitcoin integer amount as a string, avoiding the rounding
 * problems typical with floating-point math.
 * @param amount the number of satoshis
 * @param pszAmountOut a pointer that will hold the output string, in
 * bitcoins. The caller frees the returned value.
 * @param decimalPlaces set to ABC_BITCOIN_DECIMAL_PLACE to convert
 * satoshis to bitcoins.
 */
tABC_CC ABC_FormatAmount(uint64_t amount,
                         char **pszAmountOut,
                         unsigned decimalPlaces,
                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_BridgeFormatAmount(amount, pszAmountOut, decimalPlaces, pError));

exit:
    return cc;
}

/**
 * Creates a receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param pDetails      Pointer to transaction details
 * @param pszRequestID  Pointer to store allocated ID for this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CreateReceiveRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 tABC_TxDetails *pDetails,
                                 char **pszRequestID,
                                 tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxCreateReceiveRequest(szUserName, szPassword, szWalletUUID, pDetails, pszRequestID, false, pError));

exit:

    return cc;
}

/**
 * Modifies a previously created receive request.
 * Note: the previous details will be free'ed so if the user is using the previous details for this request
 * they should not assume they will be valid after this call.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pDetails      Pointer to transaction details
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ModifyReceiveRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 const char *szRequestID,
                                 tABC_TxDetails *pDetails,
                                 tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxModifyReceiveRequest(szUserName, szPassword, szWalletUUID, szRequestID, pDetails, pError));

exit:

    return cc;
}

/**
 * Finalizes a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_FinalizeReceiveRequest(const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   const char *szRequestID,
                                   tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxFinalizeReceiveRequest(szUserName, szPassword, szWalletUUID, szRequestID, pError));
exit:

    return cc;
}

/**
 * Cancels a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CancelReceiveRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 const char *szRequestID,
                                 tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxCancelReceiveRequest(szUserName, szPassword, szWalletUUID, szRequestID, pError));

exit:

    return cc;
}

/**
 * Generate the QR code for a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pszURI        Pointer to string to store URI -optional
 * @param paData        Pointer to store array of data bytes (0x0 white, 0x1 black)
 * @param pWidth        Pointer to store width of image (image will be square)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GenerateRequestQRCode(const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletUUID,
                                  const char *szRequestID,
                                  char **pszURI,
                                  unsigned char **paData,
                                  unsigned int *pWidth,
                                  tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxGenerateRequestQRCode(szUserName, szPassword, szWalletUUID, szRequestID, pszURI, paData, pWidth, pError));

exit:
    return cc;
}

/**
 * Initiates a send request.
 *
 * Once the given send has been submitted to the block chain, the given callback will
 * be called and the results data will have a pointer to the request id
 *
 * @param szUserName        UserName for the account associated with this request
 * @param szPassword        Password for the account associated with this request
 * @param szWalletUUID      UUID of the wallet associated with this request
 * @param szDestAddress     Bitcoin address (Base58) to which the funds are to be sent
 * @param pDetails          Pointer to transaction details
 * @param fRequestCallback  The function that will be called when the send request process has finished.
 * @param pData             Pointer to data to be returned back in callback,
 *                          or a `char **pszTxID` if callbacks aren't used.
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_InitiateSendRequest(const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                const char *szDestAddress,
                                tABC_TxDetails *pDetails,
                                tABC_Request_Callback fRequestCallback,
                                void *pData,
                                tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxSendInfo *pTxSendInfo = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet name provided");
    ABC_CHECK_NULL(pDetails);

    ABC_CHECK_RET(ABC_TxSendInfoAlloc(&pTxSendInfo,
                                      szUserName,
                                      szPassword,
                                      szWalletUUID,
                                      szDestAddress,
                                      pDetails,
                                      fRequestCallback,
                                      pData,
                                      pError));

    if (fRequestCallback)
    {
        pthread_t handle;
        if (!pthread_create(&handle, NULL, ABC_TxSendThreaded, pTxSendInfo))
        {
            pthread_detach(handle);
        }
    }
    else
    {
        cc = ABC_TxSend(pTxSendInfo, (char**)pData, pError);
        ABC_TxSendInfoFree(pTxSendInfo);
    }

exit:

    return cc;
}

/**
 * Initiates a transfer request.
 *
 * Once the given send has been submitted to the block chain, the given callback will
 * be called and the results data will have a pointer to the request id
 *
 * @param szUserName        UserName for the account associated with this request
 * @param szPassword        Password for the account associated with this request
 * @param pTransfer         Struct container src and dest wallet info
 * @param szDestWalletUUID  UUID of the destination wallet
 * @param pDetails          Pointer to transaction details
 * @param fRequestCallback  The function that will be called when the send request process has finished.
 * @param pData             Pointer to data to be returned back in callback,
 *                          or a `char **pszTxID` if callbacks aren't used.
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_InitiateTransfer(const char *szUserName,
                             const char *szPassword,
                             tABC_TransferDetails *pTransfer,
                             tABC_TxDetails *pDetails,
                             tABC_Request_Callback fRequestCallback,
                             void *pData,
                             tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxSendInfo *pTxSendInfo = NULL;

    char *szRequestId = NULL;
    char *szRequestAddress = NULL;
    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(pTransfer->szSrcWalletUUID);
    ABC_CHECK_ASSERT(strlen(pTransfer->szSrcWalletUUID) > 0, ABC_CC_Error, "No wallet name provided");
    ABC_CHECK_NULL(pTransfer->szDestWalletUUID);
    ABC_CHECK_ASSERT(strlen(pTransfer->szDestWalletUUID) > 0, ABC_CC_Error, "No destination wallet name provided");
    ABC_CHECK_NULL(pDetails);

    ABC_CHECK_RET(ABC_TxCreateReceiveRequest(szUserName, szPassword,
                                             pTransfer->szDestWalletUUID, pDetails,
                                             &szRequestId, true, pError));
    ABC_CHECK_RET(ABC_GetRequestAddress(szUserName, szPassword,
                                        pTransfer->szDestWalletUUID, szRequestId,
                                        &szRequestAddress, pError));
    ABC_CHECK_RET(ABC_TxSendInfoAlloc(&pTxSendInfo,
                                      szUserName,
                                      szPassword,
                                      pTransfer->szSrcWalletUUID,
                                      szRequestAddress,
                                      pDetails,
                                      fRequestCallback,
                                      pData,
                                      pError));
    pTxSendInfo->bTransfer = true;
    ABC_STRDUP(pTxSendInfo->szDestWalletUUID, pTransfer->szDestWalletUUID);
    ABC_STRDUP(pTxSendInfo->szDestName, pTransfer->szDestName);
    ABC_STRDUP(pTxSendInfo->szDestCategory, pTransfer->szDestCategory);
    ABC_STRDUP(pTxSendInfo->szSrcName, pTransfer->szSrcName);
    ABC_STRDUP(pTxSendInfo->szSrcCategory, pTransfer->szSrcCategory);
    if (fRequestCallback)
    {
        pthread_t handle;
        if (!pthread_create(&handle, NULL, ABC_TxSendThreaded, pTxSendInfo))
        {
            pthread_detach(handle);
        }
    }
    else
    {
        cc = ABC_TxSend(pTxSendInfo, (char**)pData, pError);
        ABC_TxSendInfoFree(pTxSendInfo);
    }

exit:
    ABC_FREE_STR(szRequestId);
    ABC_FREE_STR(szRequestAddress);

    return cc;
}

tABC_CC ABC_CalcSendFees(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletUUID,
                         const char *szDestAddress,
                         bool bTransfer,
                         tABC_TxDetails *pDetails,
                         int64_t *pTotalFees,
                         tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szRequestId = NULL;
    char *szRequestAddress = NULL;
    tABC_TxSendInfo *pTxSendInfo = NULL;
    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet name provided");
    ABC_CHECK_NULL(pDetails);

    ABC_ALLOC(pTxSendInfo, sizeof(tABC_TxSendInfo));
    ABC_STRDUP(pTxSendInfo->szUserName, szUserName);
    ABC_STRDUP(pTxSendInfo->szPassword, szPassword);
    ABC_STRDUP(pTxSendInfo->szWalletUUID, szWalletUUID);
    ABC_STRDUP(pTxSendInfo->szDestAddress, szDestAddress);
    pTxSendInfo->bTransfer = bTransfer;

    if (bTransfer)
    {
        ABC_CHECK_RET(ABC_TxCreateReceiveRequest(szUserName, szPassword,
                                                 pTxSendInfo->szDestAddress, pDetails,
                                                 &szRequestId, true, pError));
        ABC_CHECK_RET(ABC_GetRequestAddress(szUserName, szPassword,
                                            pTxSendInfo->szDestAddress, szRequestId,
                                            &szRequestAddress, pError));
        ABC_FREE_STR(pTxSendInfo->szDestAddress);
        ABC_STRDUP(pTxSendInfo->szDestAddress, szRequestAddress);
    }

    ABC_CHECK_RET(ABC_TxDupDetails(&(pTxSendInfo->pDetails), pDetails, pError));
    ABC_CHECK_RET(ABC_TxCalcSendFees(pTxSendInfo, pTotalFees, pError));

    pDetails->amountFeesAirbitzSatoshi = pTxSendInfo->pDetails->amountFeesAirbitzSatoshi;
    pDetails->amountFeesMinersSatoshi = pTxSendInfo->pDetails->amountFeesMinersSatoshi;
exit:
    ABC_FREE_STR(szRequestId);
    ABC_FREE_STR(szRequestAddress);
    ABC_TxSendInfoFree(pTxSendInfo);
    return cc;
}

tABC_CC ABC_MaxSpendable(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletUUID,
                         const char *szDestAddress,
                         bool bTransfer,
                         uint64_t *pMaxSatoshi,
                         tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_RET(
        ABC_BridgeMaxSpendable(szUserName, szPassword, szWalletUUID,
                               szDestAddress, bTransfer, pMaxSatoshi, pError));
exit:
    return cc;
}

/**
 * Gets the transaction specified
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szID              ID of the transaction
 * @param ppTransaction     Location to store allocated transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetTransaction(const char *szUserName,
                           const char *szPassword,
                           const char *szWalletUUID,
                           const char *szID,
                           tABC_TxInfo **ppTransaction,
                           tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxGetTransaction(szUserName, szPassword, szWalletUUID, szID, ppTransaction, pError));

exit:
    return cc;
}

/**
 * Gets the transactions associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetTransactions(const char *szUserName,
                            const char *szPassword,
                            const char *szWalletUUID,
                            tABC_TxInfo ***paTransactions,
                            unsigned int *pCount,
                            tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxGetTransactions(szUserName, szPassword, szWalletUUID, paTransactions, pCount, pError));

exit:
    return cc;
}

/**
 * Searches the transactions associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szQuery           String to match transactions against
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SearchTransactions(const char *szUserName,
                               const char *szPassword,
                               const char *szWalletUUID,
                               const char *szQuery,
                               tABC_TxInfo ***paTransactions,
                               unsigned int *pCount,
                               tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(
            ABC_TxSearchTransactions(szUserName, szPassword, szWalletUUID,
                                     szQuery, paTransactions,
                                     pCount, pError));
exit:
    return cc;
}

/**
 * Frees the given transactions
 *
 * @param pTransaction Pointer to transaction
 */
void ABC_FreeTransaction(tABC_TxInfo *pTransaction)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxFreeTransaction(pTransaction);
}

/**
 * Frees the given array of transactions
 *
 * @param aTransactions Array of transactions
 * @param count         Number of transactions
 */
void ABC_FreeTransactions(tABC_TxInfo **aTransactions,
                            unsigned int count)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxFreeTransactions(aTransactions, count);
}

/**
 * Sets the details for a specific transaction.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szID              ID of the transaction
 * @param pDetails          Details for the transaction
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_SetTransactionDetails(const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletUUID,
                                  const char *szID,
                                  tABC_TxDetails *pDetails,
                                  tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxSetTransactionDetails(szUserName, szPassword, szWalletUUID, szID, pDetails, pError));

exit:
    return cc;
}

/**
 * Gets the details for a specific existing transaction.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szID              ID of the transaction
 * @param ppDetails         Location to store allocated details for the transaction
 *                          (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetTransactionDetails(const char *szUserName,
                                    const char *szPassword,
                                    const char *szWalletUUID,
                                    const char *szID,
                                    tABC_TxDetails **ppDetails,
                                    tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxGetTransactionDetails(szUserName, szPassword, szWalletUUID, szID, ppDetails, pError));

exit:
    return cc;
}

/**
 * Gets the bit coin public address for a specified request
 *
 * @param szUserName        UserName for the account associated with the requests
 * @param szPassword        Password for the account associated with the requests
 * @param szWalletUUID      UUID of the wallet associated with the requests
 * @param szRequestID       ID of request
 * @param pszAddress        Location to store allocated address string (caller must free)
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetRequestAddress(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              const char *szRequestID,
                              char **pszAddress,
                              tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxGetRequestAddress(szUserName, szPassword, szWalletUUID, szRequestID, pszAddress, pError));

exit:
    return cc;
}

/**
 * Gets the pending requests associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the requests
 * @param szPassword        Password for the account associated with the requests
 * @param szWalletUUID      UUID of the wallet associated with the requests
 * @param paTransactions    Pointer to store array of requests info pointers
 * @param pCount            Pointer to store number of requests
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_GetPendingRequests(const char *szUserName,
                               const char *szPassword,
                               const char *szWalletUUID,
                               tABC_RequestInfo ***paRequests,
                               unsigned int *pCount,
                               tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxGetPendingRequests(szUserName, szPassword, szWalletUUID, paRequests, pCount, pError));

exit:
    return cc;
}

/**
 * Frees the given array of requets
 *
 * @param aRequests Array of requests
 * @param count     Number of requests
 */
void ABC_FreeRequests(tABC_RequestInfo **aRequests,
                      unsigned int count)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxFreeRequests(aRequests, count);
}

/**
 * Duplicates transaction details.
 * This can be used when changing the details on a transaction.
 *
 * @param ppNewDetails  Address to store pointer to copy of details
 * @param pOldDetails   Ptr to details to copy
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_DuplicateTxDetails(tABC_TxDetails **ppNewDetails,
                               const tABC_TxDetails *pOldDetails,
                               tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxDupDetails(ppNewDetails, pOldDetails, pError));

exit:

    return cc;
}

/**
 * Frees the given transaction details
 *
 * @param pDetails Ptr to details to free
 */
void ABC_FreeTxDetails(tABC_TxDetails *pDetails)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_TxFreeDetails(pDetails);
}

/**
 * Gets password rules results for a given password.
 *
 * This function takes a password and evaluates how long it will take to crack as well as
 * returns an array of rules with information on whether it satisfied each rule.
 *
 * @param szPassword        Paassword to check.
 * @param pSecondsToCrack   Location to store the number of seconds it would take to crack the password
 * @param paRules           Pointer to store the allocated array of password rules
 * @param pCountRules       Pointer to store number of password rules in the array
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CheckPassword(const char *szPassword,
                          double *pSecondsToCrack,
                          tABC_PasswordRule ***paRules,
                          unsigned int *pCountRules,
                          tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    double secondsToCrack;
    tABC_PasswordRule **aRules = NULL;
    unsigned int count = 0;
    tABC_PasswordRule *pRuleCount = NULL;
    tABC_PasswordRule *pRuleLC = NULL;
    tABC_PasswordRule *pRuleUC = NULL;
    tABC_PasswordRule *pRuleNum = NULL;
    tABC_PasswordRule *pRuleSpec = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(pSecondsToCrack);
    ABC_CHECK_NULL(paRules);
    ABC_CHECK_NULL(pCountRules);

    // we know there will be 5 rules (lots of magic numbers in this function...sorry)
    ABC_ALLOC(aRules, sizeof(tABC_PasswordRule *) * 5);

    // must have upper case letter
    ABC_ALLOC(pRuleUC, sizeof(tABC_PasswordRule));
    pRuleUC->szDescription = "Must have at least one upper case letter";
    pRuleUC->bPassed = false;
    aRules[count] = pRuleUC;
    count++;

    // must have lower case letter
    ABC_ALLOC(pRuleLC, sizeof(tABC_PasswordRule));
    pRuleLC->szDescription = "Must have at least one lower case letter";
    pRuleLC->bPassed = false;
    aRules[count] = pRuleLC;
    count++;

    // must have number
    ABC_ALLOC(pRuleNum, sizeof(tABC_PasswordRule));
    pRuleNum->szDescription = "Must have at least one number";
    pRuleNum->bPassed = false;
    aRules[count] = pRuleNum;
    count++;

    // must have special character
    ABC_ALLOC(pRuleSpec, sizeof(tABC_PasswordRule));
    pRuleSpec->szDescription = "Must have at least one special character";
    pRuleSpec->bPassed = false;
    aRules[count] = pRuleSpec;
    count++;

    // must have 10 characters
    ABC_ALLOC(pRuleCount, sizeof(tABC_PasswordRule));
    pRuleCount->szDescription = "Must have at least 10 characters";
    pRuleCount->bPassed = false;
    aRules[count] = pRuleCount;
    count++;

    // check the length
    if (strlen(szPassword) >= 10)
    {
        pRuleCount->bPassed = true;
    }

    // check the other rules
    for (int i = 0; i < strlen(szPassword); i++)
    {
        char c = szPassword[i];
        if (isdigit(c))
        {
            pRuleNum->bPassed = true;
        }
        else if (isalpha(c))
        {
            if (islower(c))
            {
                pRuleLC->bPassed = true;
            }
            else
            {
                pRuleUC->bPassed = true;
            }
        }
        else
        {
            pRuleSpec->bPassed = true;
        }
    }

    // calculate the time to crack

    /*
     From: http://blog.shay.co/password-entropy/
        A common and easy way to estimate the strength of a password is its entropy.
        The entropy is given by H=LlogBase2(N) where L is the length of the password and N is the size of the alphabet, and it is usually measured in bits.
        The entropy measures the number of bits it would take to represent every password of length L under an alphabet with N different symbols.

        For example, a password of 7 lower-case characters (such as: example, polmnni, etc.) has an entropy of H=7logBase2(26)≈32.9bits.
        A password of 10 alpha-numeric characters (such as: P4ssw0Rd97, K5lb42eQa2) has an entropy of H=10logBase2(62)≈59.54bits.

        Entropy makes it easy to compare password strengths, higher entropy means stronger password (in terms of resistance to brute force attacks).
     */
    // Note: (a) the following calculation of is just based upon one method
    //       (b) the guesses per second is arbitrary
    //       (c) it does not take dictionary attacks into account
    int L = (int) strlen(szPassword);
    if (L > 0)
    {
        int N = 0;
        if (pRuleLC->bPassed)
        {
            N += 26; // number of lower-case letters
        }
        if (pRuleUC->bPassed)
        {
            N += 26; // number of upper-case letters
        }
        if (pRuleNum->bPassed)
        {
            N += 10; // number of numeric charcters
        }
        if (pRuleSpec->bPassed)
        {
            N += 35; // number of non-alphanumeric characters on keyboard (iOS)
        }
        const double guessesPerSecond = 1000.0; // this can be changed based upon the speed of the computer
        // log2(x) = ln(x)/ln(2) = ln(x)*1.442695041
        double entropy = (double) L * log(N) * 1.442695041;
        double vars = pow(2, entropy);
        secondsToCrack = vars / guessesPerSecond;
    }
    else
    {
        secondsToCrack = 0;
    }

    // store final values
    *pSecondsToCrack = secondsToCrack;
    *paRules = aRules;
    aRules = NULL;
    *pCountRules = count;
    count = 0;

exit:
    ABC_FreePasswordRuleArray(aRules, count);

    return cc;
}

/**
 * Free the password rule array.
 *
 * This function frees the password rule array returned from ABC_CheckPassword.
 *
 * @param aRules   Array of pointers to password rules to be free'd
 * @param nCount   Number of elements in the array
 */
void ABC_FreePasswordRuleArray(tABC_PasswordRule **aRules,
                               unsigned int nCount)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    if ((aRules != NULL) && (nCount > 0))
    {
        for (int i = 0; i < nCount; i++)
        {
            // note we aren't free'ing the string because it uses heap strings
            ABC_CLEAR_FREE(aRules[i], sizeof(tABC_PasswordRule));
        }
        ABC_CLEAR_FREE(aRules, sizeof(tABC_PasswordRule *) * nCount);
    }
}

/**
 * Loads the settings for a specific account
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param szPassword    Password for the account associated with the settings
 * @param ppSettings    Location to store ptr to allocated settings (caller must free)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_LoadAccountSettings(const char *szUserName,
                                const char *szPassword,
                                tABC_AccountSettings **ppSettings,
                                tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_SyncKeys *pKeys = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountSettingsLoad(pKeys, ppSettings, pError));

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);

    return cc;
}

/**
 * Updates the settings for a specific account
 *
 * @param szUserName    UserName for the account associated with the settings
 * @param szPassword    Password for the account associated with the settings
 * @param pSettings    Pointer to settings
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_UpdateAccountSettings(const char *szUserName,
                                  const char *szPassword,
                                  tABC_AccountSettings *pSettings,
                                  tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_SyncKeys *pKeys = NULL;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_LoginGetSyncKeys(szUserName, szPassword, &pKeys, pError));
    ABC_CHECK_RET(ABC_AccountSettingsSave(pKeys, pSettings, pError));

exit:
    if (pKeys)          ABC_SyncFreeKeys(pKeys);

    return cc;
}

/**
 * Frees the given account settings
 *
 * @param pSettings Ptr to setting to free
 */
void ABC_FreeAccountSettings(tABC_AccountSettings *pSettings)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_AccountSettingsFree(pSettings);
}

/**
 * Run sync on all directories
 *
 * @param szUserName UserName for the account
 * @param szPassword Password for the account
 */
tABC_CC ABC_DataSyncAll(const char *szUserName, const char *szPassword, tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    int accountDirty = 0;
    int walletDirty = 0;

    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    tABC_CC fetchCC = ABC_LoginUpdateLoginPackageFromServer(szUserName, szPassword, pError);

    // Try fetch login package, if it fails, notify password change
    if (fetchCC == ABC_CC_BadPassword)
    {
        if (gfAsyncBitCoinEventCallback)
        {
            tABC_AsyncBitCoinInfo info;
            info.eventType = ABC_AsyncEventType_RemotePasswordChange;
            ABC_STRDUP(info.szDescription, "Password changed");
            gfAsyncBitCoinEventCallback(&info);
            ABC_FREE_STR(info.szDescription);
        }
    }
    else
    {
        // Sync the account data
        ABC_CHECK_RET(ABC_LoginSyncData(szUserName, szPassword, &accountDirty, pError));
        // Sync Wallet Data
        ABC_CHECK_RET(ABC_WalletSyncAll(szUserName, szPassword, &walletDirty, pError));
        if ((accountDirty || walletDirty) && gfAsyncBitCoinEventCallback)
        {
            tABC_AsyncBitCoinInfo info;
            info.eventType = ABC_AsyncEventType_DataSyncUpdate;
            ABC_STRDUP(info.szDescription, "Data Updated");
            gfAsyncBitCoinEventCallback(&info);
            ABC_FREE_STR(info.szDescription);
        }
    }
exit:
    return cc;
}

/**
 * Get the status of the watcher
 *
 * @param szWalletUUID Used to lookup the watcher with the data
 */
tABC_CC ABC_WatcherStatus(const char *szWalletUUID, tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet uuid provided");

    cc = ABC_BridgeWatcherStatus(szWalletUUID, pError);
exit:

    return cc;
}

/**
 * Start the watcher for a wallet
 *
 * @param szUserName   UserName for the account
 * @param szPassword   Password for the account
 * @param szWalletUUID The wallet watcher to use
 */
tABC_CC ABC_WatcherStart(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletUUID,
                         tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_BridgeWatcherStart(szUserName, szPassword, szWalletUUID, pError));

exit:

    return cc;
}

/**
 * Stop the watcher for a wallet
 *
 * @param szUserName   UserName for the account
 * @param szPassword   Password for the account
 * @param szWalletUUID The wallet watcher to use
 */
tABC_CC ABC_WatchAddresses(const char *szUserName, const char *szPassword,
                           const char *szWalletUUID, tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_TxWatchAddresses(szUserName, szPassword, szWalletUUID, pError));

exit:

    return cc;
}

/**
 * Stop the watcher for a wallet
 *
 * @param szWalletUUID The wallet watcher to use
 */
tABC_CC ABC_WatcherStop(const char *szWalletUUID, tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_BridgeWatcherStop(szWalletUUID, pError));

exit:

    return cc;
}

/**
 * Restart the watcher for a wallet
 *
 * @param szUserName   UserName for the account
 * @param szPassword   Password for the account
 * @param szWalletUUID The wallet watcher to use
 * @param clearCache   true if you want to rebuild the watcher cache
 */
tABC_CC ABC_WatcherRestart(const char *szUserName,
                           const char *szPassword,
                           const char *szWalletUUID,
                           bool clearCache,
                           tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_RET(ABC_BridgeWatcherRestart(szUserName, szPassword, szWalletUUID, clearCache, pError));

exit:

    return cc;
}

/**
 * Lookup the transaction height
 *
 * @param szWalletUUID Used to lookup the watcher with the data
 * @param szTxId The "malleable" transaction id
 * @param height Pointer to integer to store the results
 */
tABC_CC ABC_TxHeight(const char *szWalletUUID, const char *szTxId,
                     unsigned int *height, tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet uuid provided");
    ABC_CHECK_NULL(szTxId);
    ABC_CHECK_ASSERT(strlen(szTxId) > 0, ABC_CC_Error, "No tx id provided");

    cc = ABC_BridgeTxHeight(szWalletUUID, szTxId, height, pError);
exit:

    return cc;
}

/**
 * Lookup the block chain height
 *
 * @param szWalletUUID Used to lookup the watcher with the data
 * @param height Pointer to integer to store the results
 */
tABC_CC ABC_BlockHeight(const char *szWalletUUID, unsigned int *height, tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");

    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet uuid provided");

    cc = ABC_BridgeTxBlockHeight(szWalletUUID, height, pError);
exit:

    return cc;
}

/**
 * Request an update to the exchange for a currency
 *
 * @param szUserName           UserName for the account
 * @param szPassword           Password for the account
 * @param currencyNum          The currency number to update
 * @param fRequestCallback     The function that will be called when the account signin process has finished.
 * @param pData                A pointer to data to be returned back in callback
 * @param pError               A pointer to the location to store the error if there is one
 */
tABC_CC
ABC_RequestExchangeRateUpdate(const char *szUserName,
                              const char *szPassword,
                              int currencyNum,
                              tABC_Request_Callback fRequestCallback,
                              void *pData,
                              tABC_Error *pError)
{
    tABC_ExchangeInfo *pInfo = NULL;
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");

    ABC_CHECK_RET(
        ABC_ExchangeAlloc(szUserName, szPassword, currencyNum,
                          fRequestCallback, pData, &pInfo, pError));
    if (fRequestCallback)
    {
        pthread_t handle;
        if (!pthread_create(&handle, NULL, ABC_ExchangeUpdateThreaded, pInfo))
        {
            pthread_detach(handle);
        }
    }
    else
    {
        cc = ABC_ExchangeUpdate(pInfo, pError);
        ABC_ExchangeFreeInfo(pInfo);
    }

exit:
    return cc;
}

tABC_CC
ABC_IsTestNet(bool *pResult, tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "The core library has not been initalized");
    *pResult = ABC_BridgeIsTestNet(*pResult, pError);
exit:
    return cc;
}

tABC_CC ABC_Version(char **szVersion, tABC_Error *pError)
{
    ABC_DebugLog("%s called", __FUNCTION__);

    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf Version = ABC_BUF_NULL;
    bool bTestnet = false;

    ABC_IsTestNet(&bTestnet, pError);
    ABC_BUF_DUP_PTR(Version, ABC_VERSION, strlen(ABC_VERSION));
    ABC_BUF_APPEND_PTR(Version, "-", 1);
    if (NETWORK_FAKE)
    {
        ABC_BUF_APPEND_PTR(Version, "fakenet", 7);
    }
    else if (bTestnet)
    {
        ABC_BUF_APPEND_PTR(Version, "testnet", 7);
    }
    else
    {
        ABC_BUF_APPEND_PTR(Version, "mainnet", 7);
    }
    // null byte
    ABC_BUF_APPEND_PTR(Version, "", 1);

    *szVersion = (char *)ABC_BUF_PTR(Version);
    ABC_BUF_CLEAR(Version);
exit:
    ABC_BUF_FREE(Version);
    return cc;
}
