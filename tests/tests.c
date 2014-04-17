#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>

#include "ABC.h"
#include "ABC_Util.h"
#include "ABC_Crypto.h"
#include "ABC_Util.h"

#undef DEBUG

#define RED  "\x1B[31m"
#define GRN  "\x1B[32m"
#define NRM  "\x1B[0m"

#define TEST_WAIT_ON_CB(codeBlock) \
    { \
        sem_wait(&cb_sem); \
        codeBlock \
        if (Error.code != ABC_CC_Ok) { \
            sem_post(&cb_sem); \
        } else { \
            sem_wait(&cb_sem); \
            sem_post(&cb_sem); \
        } \
    }


#if 0
  #define WRAP_PRINTF(a) printf a
#else
  #define WRAP_PRINTF(a) (void)0
#endif

static char *USERNAME;
static char *PASSWORD;
static char *WALLET_UUID;
static const char *WALLET_NAME = "My Wallet";
static int passed = 0;
static int failed = 0;
static sem_t cb_sem;

static void printABC_Error(const tABC_Error *pError)
{
    if (pError)
    {
        if (pError->code != ABC_CC_Ok)
        {
            printf("%sFailed!%s\n", RED, NRM);
            printf("Code: %d, Desc: %s, Func: %s, File: %s, Line: %d\n",
                   pError->code,
                   pError->szDescription,
                   pError->szSourceFunc,
                   pError->szSourceFile,
                   pError->nSourceLine
                   );
            failed++;
        } else {
            printf("%sPassed!%s\n", GRN, NRM);
            passed++;
        }
        printf("\n");
    }
}

static void ABC_BitCoin_Event_Callback(const tABC_AsyncBitCoinInfo *pInfo)
{
    printf("ABC_BitCoin_Event_Callback\n");
}

static void ABC_Request_Callback(const tABC_RequestResults *pResults)
{
    printf("ABC_Request_Callback\n");
    printABC_Error(&(pResults->errorInfo));

    /* Release semaphore, and continue with tests */
    sem_post(&cb_sem);
}

static void ABC_WalletCreate_Callback(const tABC_RequestResults *pResults)
{
    printf("ABC_WalletCreate_Callback\n");
    printf("%s\n", (char *) pResults->pRetData);

    size_t slen = strlen((char *) pResults->pRetData) + 1;
    WALLET_UUID = (char *) malloc(sizeof(char) * slen);
    snprintf(WALLET_UUID, slen, "%s", (char *) pResults->pRetData);

    printABC_Error(&(pResults->errorInfo));

    /* Release semaphore, and continue with tests */
    sem_post(&cb_sem);
}

static void test_initialize()
{
    tABC_Error Error;
    size_t size = 4;
    const char *datapath = "/tmp";
    char unsigned seed[] = "abcd";
    ABC_Initialize(datapath,
                   ABC_BitCoin_Event_Callback,
                   NULL,
                   seed,
                   size,
                   &Error);
    printABC_Error(&Error);
}

static void test_createaccount()
{
    tABC_Error Error;
    TEST_WAIT_ON_CB(
        ABC_CreateAccount(USERNAME,
                        PASSWORD,
                        (char *)"1234",
                        ABC_Request_Callback,
                        NULL,
                        &Error);
        printABC_Error(&Error);
    )
}

static void test_signin()
{
    tABC_Error Error;
    TEST_WAIT_ON_CB(
        ABC_SignIn(USERNAME,
                PASSWORD,
                ABC_Request_Callback,
                NULL,
                &Error);
        printABC_Error(&Error);
    )
}

static void test_setrecovery()
{
    tABC_Error Error;
    TEST_WAIT_ON_CB(
        ABC_SetAccountRecoveryQuestions(
            USERNAME,
            PASSWORD,
            (char *)"Question1\nQuestion2\nQuestion3\nQuestion4\nQuestion5",
            (char *)"Answer1\nAnswer2\nAnswer3\nAnswer4\nAnswer5",
            ABC_Request_Callback,
            NULL,
            &Error);
        printABC_Error(&Error);
    )
}

static void test_createwallet()
{
    tABC_Error Error;
    TEST_WAIT_ON_CB(
        ABC_CreateWallet(USERNAME,
                        PASSWORD,
                        WALLET_NAME,
                        840, // currency num
                        0, // attributes
                        ABC_WalletCreate_Callback,
                        NULL,
                        &Error);
        printABC_Error(&Error);
    )
}


static void test_changesettings()
{
    tABC_Error Error;
    Error.code = ABC_CC_Ok;

    tABC_AccountSettings *pNewSettings = NULL;
    ABC_LoadAccountSettings(USERNAME,
                            PASSWORD,
                            &pNewSettings,
                            &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("Updating settings...\n"));

    if (pNewSettings != NULL)
    {
        if (pNewSettings->szFirstName) free(pNewSettings->szFirstName);
        pNewSettings->szFirstName = strdup("Adam");
        if (pNewSettings->szLastName)
            free(pNewSettings->szLastName);
        pNewSettings->szLastName = strdup("Harris");
        if (pNewSettings->szNickname)
            free(pNewSettings->szNickname);
        pNewSettings->szNickname = strdup("AdamDNA");
        pNewSettings->bNameOnPayments = true;
        pNewSettings->minutesAutoLogout = 30;
        if (pNewSettings->szLanguage)
            free(pNewSettings->szLanguage);
        pNewSettings->szLanguage = strdup("en");
        pNewSettings->currencyNum = 840;
        pNewSettings->bAdvancedFeatures = true;
        if (pNewSettings->bitcoinDenomination.szLabel)
            free(pNewSettings->bitcoinDenomination.szLabel);
        pNewSettings->bitcoinDenomination.szLabel = strdup("BTC");
        pNewSettings->bitcoinDenomination.satoshi = 100000000;
        if (pNewSettings->exchangeRateSources.numSources > 0
            && pNewSettings->exchangeRateSources.aSources)
        {
            unsigned int i;
            for (i = 0; i < pNewSettings->exchangeRateSources.numSources; i++)
            {
                if (pNewSettings->exchangeRateSources.aSources[i]->szSource)
                    free(pNewSettings->exchangeRateSources.aSources[i]->szSource);
                free(pNewSettings->exchangeRateSources.aSources[i]);
            }
            free(pNewSettings->exchangeRateSources.aSources);
        }
        pNewSettings->exchangeRateSources.numSources = 2;
        pNewSettings->exchangeRateSources.aSources =
            (tABC_ExchangeRateSource**)
                calloc(1, pNewSettings->exchangeRateSources.numSources *
                          sizeof(tABC_ExchangeRateSource *));
        pNewSettings->exchangeRateSources.aSources[0] =
            (tABC_ExchangeRateSource*)
                calloc(1, sizeof(tABC_ExchangeRateSource));
        pNewSettings->exchangeRateSources.aSources[0]->currencyNum = 840;
        pNewSettings->exchangeRateSources.aSources[0]->szSource = strdup("bitstamp");

        pNewSettings->exchangeRateSources.aSources[1] =
            (tABC_ExchangeRateSource *)
            calloc(1, sizeof(tABC_ExchangeRateSource));
        pNewSettings->exchangeRateSources.aSources[1]->currencyNum = 124;
        pNewSettings->exchangeRateSources.aSources[1]->szSource = strdup("cavirtex");

        ABC_UpdateAccountSettings(USERNAME, PASSWORD, pNewSettings, &Error);
        printABC_Error(&Error);
        ABC_FreeAccountSettings(pNewSettings);
    }
}

static void test_loadsettings()
{
    tABC_Error Error;
    tABC_AccountSettings *pSettings = NULL;
    ABC_LoadAccountSettings(USERNAME, PASSWORD, &pSettings, &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("Settings: \n"));

    if (pSettings != NULL)
    {
        WRAP_PRINTF(("First name: %s\n", pSettings->szFirstName ? pSettings->szFirstName : "(none)"));
        WRAP_PRINTF(("Last name: %s\n", pSettings->szLastName ? pSettings->szLastName : "(none)"));
        WRAP_PRINTF(("Nickname: %s\n", pSettings->szNickname ? pSettings->szNickname : "(none)"));
        WRAP_PRINTF(("List name on payments: %s\n", pSettings->bNameOnPayments ? "yes" : "no"));
        WRAP_PRINTF(("Minutes before auto logout: %d\n", pSettings->minutesAutoLogout));
        WRAP_PRINTF(("Language: %s\n", pSettings->szLanguage));
        WRAP_PRINTF(("Currency num: %d\n", pSettings->currencyNum));
        WRAP_PRINTF(("Advanced features: %s\n", pSettings->bAdvancedFeatures ? "yes" : "no"));
        WRAP_PRINTF(("Denomination satoshi: %ld\n", pSettings->bitcoinDenomination.satoshi));
        WRAP_PRINTF(("Denomination label: %s\n", pSettings->bitcoinDenomination.szLabel));
        WRAP_PRINTF(("Exchange rate sources:\n"));
        unsigned int i;
        for (i = 0; i < pSettings->exchangeRateSources.numSources; i++)
        {
            WRAP_PRINTF(("\tcurrency: %d\tsource: %s\n", pSettings->exchangeRateSources.aSources[i]->currencyNum, pSettings->exchangeRateSources.aSources[i]->szSource));
        }

        ABC_FreeAccountSettings(pSettings);
    }
}

static void test_receive_request()
{
    tABC_Error Error;
    tABC_TxDetails Details;
    Details.amountSatoshi = 100;
    Details.amountCurrency = 8.8;
    Details.szName = "MyName";
    Details.szCategory = "MyCategory";
    Details.szNotes = "MyNotes";
    Details.attributes = 0x1;

    char *szRequestID = NULL;

    ABC_CreateReceiveRequest(USERNAME, PASSWORD, WALLET_UUID,
                             &Details, &szRequestID, &Error);
    printABC_Error(&Error);

    if (szRequestID)
    {
        WRAP_PRINTF(("Request created: %s\n", szRequestID));
    }
}

static void test_checkpassword()
{
    tABC_Error Error;
    tABC_PasswordRule **aRules = NULL;
    unsigned int count = 0;
    double secondsToCrack;

    ABC_CheckPassword("TEST TEXT",
                      &secondsToCrack,
                      &aRules,
                      &count,
                      &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("Password results:\n"));
    WRAP_PRINTF(("Time to crack: %lf seconds\n", secondsToCrack));
    unsigned int i;
    for (i = 0; i < count; i++)
    {
        tABC_PasswordRule *pRule = aRules[i];
        WRAP_PRINTF(("%s - %s\n", pRule->bPassed ? "pass" : "fail", pRule->szDescription));
    }

    ABC_FreePasswordRuleArray(aRules, count);
}


static void test_cancel_request()
{
    tABC_Error Error;
    ABC_CancelReceiveRequest(USERNAME, PASSWORD, WALLET_UUID, "0", &Error);
    printABC_Error(&Error);
}

static void test_finalizerequest()
{
    tABC_Error Error;
    ABC_FinalizeReceiveRequest(USERNAME, PASSWORD, WALLET_UUID, "0", &Error);
    printABC_Error(&Error);
}

static void test_pendingrequests()
{
    tABC_Error Error;
    tABC_RequestInfo **aRequests = NULL;
    unsigned int nCount = 0;
    ABC_GetPendingRequests(USERNAME, PASSWORD, WALLET_UUID, &aRequests,
                           &nCount, &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("Pending requests:\n"));

    if (nCount > 0)
    {

        // list them
        unsigned int i;
        for (i = 0; i < nCount; i++)
        {
            tABC_RequestInfo *pInfo = aRequests[i];

            WRAP_PRINTF(("Pending Request: %s, time: %ld, satoshi: %ld, currency: %lf, name: %s, category: %s, notes: %s, attributes: %u, existing_satoshi: %ld, owed_satoshi: %ld\n",
                   pInfo->szID,
                   pInfo->timeCreation,
                   pInfo->pDetails->amountSatoshi,
                   pInfo->pDetails->amountCurrency,
                   pInfo->pDetails->szName,
                   pInfo->pDetails->szCategory,
                   pInfo->pDetails->szNotes,
                   pInfo->pDetails->attributes,
                   pInfo->amountSatoshi,
                   pInfo->owedSatoshi));
        }

        // take the first one and duplicate the info
        tABC_TxDetails *pNewDetails;
        ABC_DuplicateTxDetails(&pNewDetails, aRequests[0]->pDetails, &Error);
        printABC_Error(&Error);

        // change the attributes
        pNewDetails->attributes++;

        // write it back out
        ABC_ModifyReceiveRequest(USERNAME,
                                 PASSWORD,
                                 "TEST TEXT",
                                 aRequests[0]->szID,
                                 pNewDetails,
                                 &Error);
        printABC_Error(&Error);

        // free the duplicated details
        ABC_FreeTxDetails(pNewDetails);
    }

    ABC_FreeRequests(aRequests, nCount);
}

static void test_transactions()
{
    tABC_Error Error;
    tABC_TxInfo **aTransactions = NULL;
    unsigned int nCount = 0;
    ABC_GetTransactions(USERNAME, PASSWORD, WALLET_UUID,
                        &aTransactions, &nCount, &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("Transactions:\n"));

    // list them
    unsigned int i;
    for (i = 0; i < nCount; i++) {
        tABC_TxInfo *pInfo = aTransactions[i];

        WRAP_PRINTF(("Transaction: %s, time: %ld, satoshi: %ld, currency: %lf, name: %s, category: %s, notes: %s, attributes: %u\n",
               pInfo->szID,
               pInfo->timeCreation,
               pInfo->pDetails->amountSatoshi,
               pInfo->pDetails->amountCurrency,
               pInfo->pDetails->szName,
               pInfo->pDetails->szCategory,
               pInfo->pDetails->szNotes,
               pInfo->pDetails->attributes));
    }
    ABC_FreeTransactions(aTransactions, nCount);
}

static void test_bitcoinuri()
{
    char *str = "bitcoin:1585j6GvTMz6gkCgjK3kpm9SBkEZCdN5aW?amount=0.00000100&label=MyName&message=MyNotes";
    tABC_Error Error;
    tABC_BitcoinURIInfo *uri;
    WRAP_PRINTF(("Parsing URI: %s\n", str));
    ABC_ParseBitcoinURI(str, &uri, &Error);
    printABC_Error(&Error);

    if (uri != NULL)
    {
        if (uri->szAddress)
            WRAP_PRINTF(("    address: %s\n", uri->szAddress));
        WRAP_PRINTF(("    amount: %ld\n", uri->amountSatoshi));
        if (uri->szLabel)
            WRAP_PRINTF(("    label: %s\n", uri->szLabel));
        if (uri->szMessage)
            WRAP_PRINTF(("    message: %s\n", uri->szMessage));
    }
    else
    {
        WRAP_PRINTF(("URI parse failed!\n"));
    }
}

static void test_qrcode()
{
    tABC_Error Error;
    unsigned int width = 0;
    unsigned char *pData = NULL;

    ABC_GenerateRequestQRCode(USERNAME, PASSWORD, WALLET_UUID,
                              "0", &pData, &width, &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("QRCode width: %d\n", width));
#ifdef DEBUG
    ABC_UtilHexDump("QRCode data", pData, width * width);
#endif
    unsigned int y;
    for (y = 0; y < width; y++)
    {
        unsigned int x;
        for (x = 0; x < width; x++)
        {
            if (pData[(y * width) + x] & 0x1)
            {
                WRAP_PRINTF(("%c", '*'));
            }
            else
            {
                WRAP_PRINTF((" "));
            }
        }
        WRAP_PRINTF(("\n"));
    }
    free(pData);
}

static void test_recoveryquestions()
{
    tABC_Error Error;
    char *szRecoveryQuestions;
    ABC_GetRecoveryQuestions(USERNAME, &szRecoveryQuestions, &Error);
    printABC_Error(&Error);

    if (szRecoveryQuestions)
    {
        WRAP_PRINTF(("Recovery questions:\n%s\n", szRecoveryQuestions));
        free(szRecoveryQuestions);
    }
    else
    {
        WRAP_PRINTF(("No recovery questions!"));
    }
}

static void test_getrecoveryquestions()
{
    tABC_Error Error;
    TEST_WAIT_ON_CB(
        ABC_GetQuestionChoices(USERNAME, ABC_Request_Callback,
                            NULL, &Error);
        printABC_Error(&Error);
    )
}

static void test_changepw_with_oldpw()
{
    tABC_Error Error;
    TEST_WAIT_ON_CB(
        ABC_ChangePassword(USERNAME, PASSWORD, PASSWORD, "4321",
            ABC_Request_Callback, NULL, &Error);
        printABC_Error(&Error);
    )
}

static void test_changepw_with_qs()
{
    tABC_Error Error;
    TEST_WAIT_ON_CB(
        ABC_ChangePasswordWithRecoveryAnswers(USERNAME,
            "Answer1\nAnswer2\nAnswer3\nAnswer4\nAnswer5", PASSWORD, "2222",
            ABC_Request_Callback, NULL, &Error);
        printABC_Error(&Error);
    )
}

static void test_listwallets()
{
    tABC_Error Error;
    tABC_WalletInfo **aWalletInfo = NULL;
    unsigned int nCount;
    ABC_GetWallets(USERNAME, PASSWORD, &aWalletInfo, &nCount, &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("Wallets:\n"));

    // list them
    unsigned int i;
    for (i = 0; i < nCount; i++)
    {
        tABC_WalletInfo *pInfo = aWalletInfo[i];

        WRAP_PRINTF(("Account: %s, UUID: %s, Name: %s, currency: %d, attributes: %u, balance: %ld\n",
               pInfo->szUserName,
               pInfo->szUUID,
               pInfo->szName,
               pInfo->currencyNum,
               pInfo->attributes,
               pInfo->balanceSatoshi));
    }

    ABC_FreeWalletInfoArray(aWalletInfo, nCount);
}

static void test_reorderwallets()
{
    tABC_Error Error;
    tABC_WalletInfo **aWalletInfo = NULL;
    unsigned int nCount;
    ABC_GetWallets(USERNAME, PASSWORD, &aWalletInfo, &nCount, &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("Wallets:\n"));

    // create an array of them in reverse order
    char **aszWallets = (char**) malloc(sizeof(char *) * nCount);
    unsigned int i;
    for (i = 0; i < nCount; i++)
    {
        tABC_WalletInfo *pInfo = aWalletInfo[i];

        WRAP_PRINTF(("Account: %s, UUID: %s, Name: %s, currency: %d, attributes: %u, balance: %ld\n",
               pInfo->szUserName,
               pInfo->szUUID,
               pInfo->szName,
               pInfo->currencyNum,
               pInfo->attributes,
               (long) pInfo->balanceSatoshi));

        aszWallets[nCount - i - 1] = strdup(pInfo->szUUID);
    }

    ABC_FreeWalletInfoArray(aWalletInfo, nCount);

    // set them in the new order
    ABC_SetWalletOrder(USERNAME, PASSWORD, aszWallets, nCount, &Error);
    printABC_Error(&Error);

    ABC_UtilFreeStringArray(aszWallets, nCount);

    ABC_GetWallets(USERNAME, PASSWORD, &aWalletInfo, &nCount, &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("Wallets:\n"));

    // create an array of them in reverse order
    for (i = 0; i < nCount; i++)
    {
        tABC_WalletInfo *pInfo = aWalletInfo[i];

        WRAP_PRINTF(("Account: %s, UUID: %s, Name: %s, currency: %d, attributes: %u, balance: %ld\n",
               pInfo->szUserName,
               pInfo->szUUID,
               pInfo->szName,
               pInfo->currencyNum,
               pInfo->attributes,
               (long) pInfo->balanceSatoshi));
    }

    ABC_FreeWalletInfoArray(aWalletInfo, nCount);
}

static void test_checkrecoveryquestions()
{
    tABC_Error Error;
    bool bValid = false;
    ABC_CheckRecoveryAnswers(USERNAME,
        (char *)"Answer1\nAnswer2\nAnswer3\nAnswer4\nAnswer5",
        &bValid, &Error);
    printABC_Error(&Error);
}

/*
static void test_categories()
{
    Error.code = ABC_CC_Ok;
    char **aszCategories;
    unsigned int count;
    NSMutableArray *arrayCategories = [[NSMutableArray alloc] init];

    ABC_GetCategories("a", &aszCategories, &count, &Error);
    int i;
    for (i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);


    ABC_AddCategory("a", "firstCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);

    ABC_AddCategory("a", "secondCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);

    ABC_AddCategory("a", "thirdCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);

    ABC_RemoveCategory("a", "secondCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);

    ABC_RemoveCategory("a", "firstCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);

    ABC_RemoveCategory("a", "thirdCategory", &Error);
    ABC_GetCategories("a", &aszCategories, &count, &Error);
    [arrayCategories removeAllObjects];
    for (i = 0; i < count; i++)
    {
        [arrayCategories addObject:[NSString stringWithFormat:@"%s", aszCategories[i]]];
    }
    ABC_UtilFreeStringArray(aszCategories, count);
}
*/

static void test_setget_pin()
{
    tABC_Error Error;
    char *szPIN = NULL;

    ABC_GetPIN(USERNAME, PASSWORD, &szPIN, &Error);
    printABC_Error(&Error);
    free(szPIN);

    WRAP_PRINTF(("test_setpin"));
    ABC_SetPIN(USERNAME, PASSWORD, "1111", &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("test_getpin"));
    ABC_GetPIN(USERNAME, PASSWORD, &szPIN, &Error);
    printABC_Error(&Error);
    free(szPIN);
}

static void test_enc_dec_string()
{
    tABC_Error Error;
    tABC_U08Buf Data;
    tABC_U08Buf Key;
    char *szDataToEncrypt = "Data to be encrypted so we can check it";
    ABC_BUF_SET_PTR(Data, (unsigned char *)szDataToEncrypt, strlen(szDataToEncrypt) + 1);
    ABC_BUF_SET_PTR(Key, (unsigned char *)"Key", strlen("Key") + 1);
    char *szEncDataJSON;
    WRAP_PRINTF(("Calling encrypt...\n"));
    WRAP_PRINTF(("          data length: %lu\n", strlen(szDataToEncrypt) + 1));
    WRAP_PRINTF(("          data: %s\n", szDataToEncrypt));
    ABC_CryptoEncryptJSONString(Data,
                                Key,
                                ABC_CryptoType_AES256,
                                &szEncDataJSON,
                                &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("JSON: \n%s\n", szEncDataJSON));

    tABC_U08Buf Data2;
    ABC_CryptoDecryptJSONString(szEncDataJSON,
                                Key,
                                &Data2,
                                &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("Decrypted data length: %d\n", (int)ABC_BUF_SIZE(Data2)));
    WRAP_PRINTF(("Decrypted data: %s\n", ABC_BUF_PTR(Data2)));

    free(szEncDataJSON);
}

static void test_enc_dec_scrypt()
{
    tABC_Error Error;
    tABC_U08Buf Data;
    tABC_U08Buf Key;
    char *szDataToEncrypt = "Data to be encrypted so we can check it";
    ABC_BUF_SET_PTR(Data, (unsigned char *)szDataToEncrypt, strlen(szDataToEncrypt) + 1);
    ABC_BUF_SET_PTR(Key, (unsigned char *)"Key", strlen("Key") + 1);
    char *szEncDataJSON;
    WRAP_PRINTF(("Calling encrypt...\n"));
    WRAP_PRINTF(("          data length: %lu\n", strlen(szDataToEncrypt) + 1));
    WRAP_PRINTF(("          data: %s\n", szDataToEncrypt));
    ABC_CryptoEncryptJSONString(Data,
                                Key,
                                ABC_CryptoType_AES256_Scrypt,
                                &szEncDataJSON,
                                &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("JSON: \n%s\n", szEncDataJSON));

    tABC_U08Buf Data2;
    ABC_CryptoDecryptJSONString(szEncDataJSON,
                                Key,
                                &Data2,
                                &Error);
    printABC_Error(&Error);

    WRAP_PRINTF(("Decrypted data length: %d\n", (int)ABC_BUF_SIZE(Data2)));
    WRAP_PRINTF(("Decrypted data: %s\n", ABC_BUF_PTR(Data2)));

    free(szEncDataJSON);
}

static void test_getcurrencies()
{
    tABC_Error Error;
    tABC_Currency *aCurrencyArray;
    int currencyCount;
    ABC_GetCurrencies(&aCurrencyArray, &currencyCount, &Error);
    int i;
    for (i = 0; i < currencyCount; i++)
    {
        WRAP_PRINTF(("%d, %s, %s, %s\n",
            aCurrencyArray[i].num,
            aCurrencyArray[i].szCode,
            aCurrencyArray[i].szDescription,
            aCurrencyArray[i].szCountries));
    }
    printABC_Error(&Error);
}

static void create_credentials()
{
    size_t sSize = 20;
    USERNAME = (char *) malloc(sSize * sizeof(char));
    PASSWORD = (char *) malloc(sSize * sizeof(char));

    snprintf(USERNAME, sSize, "login%ld", time(NULL));
    snprintf(PASSWORD, sSize, "pass%ld", time(NULL));

    WRAP_PRINTF(("Username: %s\n", USERNAME));
    WRAP_PRINTF(("Password: %s\n", PASSWORD));
}

static void destroy_credentials()
{
    free(USERNAME);
    free(PASSWORD);
    free(WALLET_UUID);
}

int main(int argc, const char *argv[])
{
    sem_init(&cb_sem, 0, 1);
    create_credentials();

    printf("test_initialize();\n");
    test_initialize();

    printf("test_createaccount();\n");
    test_createaccount();

    printf("test_signin();\n");
    test_signin();

    printf("test_setrecovery();\n");
    test_setrecovery();

    printf("test_createwallet();\n");
    test_createwallet();

    printf("test_changesettings();\n");
    test_changesettings();

    printf("test_loadsettings();\n");
    test_loadsettings();

    printf("test_receive_request();\n");
    test_receive_request();

    printf("test_checkpassword();\n");
    test_checkpassword();

    printf("test_cancel_request();\n");
    test_cancel_request();

    printf("test_pendingrequests();\n");
    test_pendingrequests();

    printf("test_transactions();\n");
    test_transactions();

    printf("test_bitcoinuri();\n");
    test_bitcoinuri();

    printf("test_qrcode();\n");
    test_qrcode();

    printf("test_recoveryquestions();\n");
    test_recoveryquestions();

    printf("test_getrecoveryquestions();\n");
    test_getrecoveryquestions();

    printf("test_changepw_with_oldpw();\n");
    test_changepw_with_oldpw();

    printf("test_changepw_with_qs();\n");
    test_changepw_with_qs();

    printf("test_listwallets();\n");
    test_listwallets();

    printf("test_reorderwallets();\n");
    test_reorderwallets();

    printf("test_checkrecoveryquestions();\n");
    test_checkrecoveryquestions();

    printf("test_enc_dec_string();\n");
    test_enc_dec_string();

    printf("test_enc_dec_scrypt();\n");
    test_enc_dec_scrypt();

    printf("test_setget_pin();\n");
    test_setget_pin();

    printf("test_finalizerequest();\n");
    test_finalizerequest();

    printf("test_getcurrencies();\n");
    test_getcurrencies();

    destroy_credentials();
    sem_destroy(&cb_sem);

    printf("Passed: %d\n", passed);
    printf("Failed: %d\n", failed);

    return failed > 0 ? 1 : 0;
}

