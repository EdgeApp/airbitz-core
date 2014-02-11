/**
 * @file
 * AirBitz Core API functions.
 *
 * This file contains all of the functions available in the AirBitz Core API.
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include "ABC.h"
#include "ABC_Account.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Wallet.h"
#include "ABC_Crypto.h"

void *threadCreateAccount(void *pData);

/** globally accessable function pointer for BitCoin event callbacks */
static tABC_BitCoin_Event_Callback gfAsyncBitCoinEventCallback = NULL;
static void *pAsyncBitCoinCallerData = NULL;



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
                       tABC_BitCoin_Event_Callback  fAsyncBitCoinEventCallback,
                       void                         *pData,
                       const unsigned char          *pSeedData,
                       unsigned int                 seedLength,
                       tABC_Error                   *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf Seed = ABC_BUF_NULL;

    gfAsyncBitCoinEventCallback = fAsyncBitCoinEventCallback;
    pAsyncBitCoinCallerData = pData;
    
    if (szRootDir)
    {
        ABC_CHECK_RET(ABC_FileIOSetRootDir(szRootDir, pError));
    }

    
    ABC_BUF_DUP_PTR(Seed, pSeedData, seedLength);
    ABC_CHECK_RET(ABC_CryptoSetRandomSeed(Seed, pError));
    
exit:
    ABC_BUF_FREE(Seed);

    return cc;
}

/**
 * Create a new account.
 *
 * This function kicks off a thread to create a new account. The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param szRecoveryQuestions       Recovery questions - newline seperated
 * @param szRecoveryAnswers         Recovery answers - newline seperated
 * @param szPIN                     PIN for the account
 * @param fCreateAcountCallback     The function that will be called when the account create process has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CreateAccount(const char *szUserName,
                          const char *szPassword,
                          const char *szRecoveryQuestions,
                          const char *szRecoveryAnswers,
                          const char *szPIN,
                          tABC_Create_Account_Callback fCreateAccountCallback,
                          void *pData,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_AccountCreateInfo *pAccountCreateInfo;
    
    ABC_CHECK_RET(ABC_AccountCreateInfoAlloc(&pAccountCreateInfo,
                                             szUserName,
                                             szPassword,
                                             szRecoveryQuestions,
                                             szRecoveryAnswers,
                                             szPIN,
                                             fCreateAccountCallback,
                                             pData,
                                             pError));
    
    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_AccountCreateThreaded, pAccountCreateInfo))
    {
        //printf("Thread create successfully !!!\n");
        if ( ! pthread_detach(handle) )
        {
            //printf("Thread detached successfully !!!\n");
        }
    }
    
exit:

    return cc;
}

/**
 * Create a new wallet.
 *
 * This function kicks off a thread to create a new wallet. The callback will be called when it has finished.
 *
 * @param szUserName                UserName for the account
 * @param szPassword                Password for the account
 * @param szWalletName              Wallet Name
 * @param szCurrencyCode            ISO 4217 currency code
 * @param fCreateAcountCallback     The function that will be called when the wallet create process has finished.
 * @param pData                     Pointer to data to be returned back in callback
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_CreateWallet(const char *szUserName,
                         const char *szPassword,
                         const char *szWalletName,
                         const char *szCurrencyCode,
                         tABC_Create_Wallet_Callback fCreateWalletCallback,
                         void *pData,
                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_WalletCreateInfo *pWalletCreateInfo;
    
    ABC_CHECK_RET(ABC_WalletCreateInfoAlloc(&pWalletCreateInfo,
                                             szUserName,
                                             szPassword,
                                             szWalletName,
                                             szCurrencyCode,
                                             fCreateWalletCallback,
                                             pData,
                                             pError));
    
    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_WalletCreateThreaded, pWalletCreateInfo))
    {
        printf("Thread create successfully !!!\n");
        if ( ! pthread_detach(handle) )
        {
            printf("Thread detached successfully !!!\n");
        }
    }
    
exit:

    return cc;
}


/**
 * Clear cached keys.
 *
 * This function clears any keys that might be cached.
 *
 * @param pError                    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_ClearKeyCache(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_AccountClearKeyCache(pError));
    
exit:

    return cc;
}


void tempEventA()
{
    if (gfAsyncBitCoinEventCallback)
    {
        tABC_AsyncBitCoinInfo info;
        info.pData = pAsyncBitCoinCallerData;
        strcpy(info.szDescription, "Event A");
        gfAsyncBitCoinEventCallback(&info);
    }
}

void tempEventB()
{
    if (gfAsyncBitCoinEventCallback)
    {
        tABC_AsyncBitCoinInfo info;
        strcpy(info.szDescription, "Event B");
        info.pData = pAsyncBitCoinCallerData;
        gfAsyncBitCoinEventCallback(&info);
    }
}
