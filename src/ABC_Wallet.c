/**
 * @file
 * AirBitz Wallet functions.
 *
 * This file contains all of the functions associated with wallet creation, 
 * viewing and modification.
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "ABC_Wallet.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Crypto.h"
#include "ABC_Account.h"

#define WALLET_KEY_LENGTH               AES_256_KEY_LENGTH

#define WALLET_DIR                      "Wallets"

static tABC_CC ABC_WalletCreate(tABC_WalletCreateInfo *pInfo, tABC_Error *pError);
static tABC_CC ABC_WalletCreateRootDir(tABC_Error *pError);
static tABC_CC ABC_WalletGetRootDirName(char **pszRootDir, tABC_Error *pError);
static tABC_CC ABC_WalletGetDirName(char **pszDir, char *szWalletUUID, tABC_Error *pError);

tABC_CC ABC_WalletCreateInfoAlloc(tABC_WalletCreateInfo **ppWalletCreateInfo,
                                   const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletName,
                                   int        currencyNum,
                                   tABC_Request_Callback fRequestCallback,
                                   void *pData,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(ppWalletCreateInfo);
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szWalletName);
    ABC_CHECK_NULL(fRequestCallback);

    tABC_WalletCreateInfo *pWalletCreateInfo = (tABC_WalletCreateInfo *) calloc(1, sizeof(tABC_WalletCreateInfo));
    
    pWalletCreateInfo->szUserName = strdup(szUserName);
    pWalletCreateInfo->szPassword = strdup(szPassword);
    pWalletCreateInfo->szWalletName = strdup(szWalletName);
    pWalletCreateInfo->currencyNum = currencyNum;
    
    pWalletCreateInfo->fRequestCallback = fRequestCallback;
    
    pWalletCreateInfo->pData = pData;
    
    *ppWalletCreateInfo = pWalletCreateInfo;
    
exit:

    return cc;
}

void ABC_WalletCreateInfoFree(tABC_WalletCreateInfo *pWalletCreateInfo)
{
    if (pWalletCreateInfo)
    {
        free((void *)pWalletCreateInfo->szUserName);
        free((void *)pWalletCreateInfo->szPassword);
        free((void *)pWalletCreateInfo->szWalletName);
        
        free(pWalletCreateInfo);
    }
}

/**
 * Create a new wallet. Assumes it is running in a thread.
 *
 * This function creates a new wallet. 
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to create a wallet (should be a tABC_WalletCreateInfo)
 */
void *ABC_WalletCreateThreaded(void *pData)
{
    tABC_WalletCreateInfo *pInfo = (tABC_WalletCreateInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;

        results.requestType = ABC_RequestType_CreateWallet;
        
        results.bSuccess = false;
        
        // create the wallet
        tABC_CC CC = ABC_WalletCreate(pInfo, &(results.errorInfo));
        results.errorInfo.code = CC;
        
        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);
        
        // it is our responsibility to free the info struct
        ABC_WalletCreateInfoFree(pInfo);
    }
    
    return NULL;
}

// creates the wallet with the given info
tABC_CC ABC_WalletCreate(tABC_WalletCreateInfo *pInfo,
                         tABC_Error            *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szWalletDir = NULL;
    char *szAccountDir = NULL;
    char *szUUID = NULL;
    tABC_U08Buf MK = ABC_BUF_NULL;
    tABC_U08Buf L2 = ABC_BUF_NULL;
    tABC_U08Buf LP2 = ABC_BUF_NULL;

    ABC_CHECK_NULL(pInfo);
    
    // get the local directory for this account
    ABC_CHECK_RET(ABC_AccountGetDirName(pInfo->szUserName, &szAccountDir, pError));
    
    // get L2
    ABC_CHECK_RET(ABC_AccountGetKey(pInfo->szUserName, pInfo->szPassword, ABC_AccountKey_L2, &L2, pError));

    // get LP2
    ABC_CHECK_RET(ABC_AccountGetKey(pInfo->szUserName, pInfo->szPassword, ABC_AccountKey_LP2, &LP2, pError));

    // create wallet guid
    ABC_CHECK_RET(ABC_CryptoGenUUIDString(&szUUID, pError));
    printf("Wallet UUID: %s\n", szUUID);

    // generate the master key for this wallet - MK_<Wallet_GUID1>
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(WALLET_KEY_LENGTH, &MK, pError));
    ABC_UtilHexDumpBuf("MK", MK);

    // TODO: add wallet to account Wallets.json

    // TODO: create wallet directory sync key 

    // TODO: create encrypted wallet directory sync key ERepoWalletKey_<Wallet_GUID1>.json ( AES256(RepoWalletKey_<Wallet_GUID1>, L2) )

    // TODO: add encrypted wallet sync key to account directory

    // TODO: create encrypted wallet key EMK_<Wallet_UUID1>.json ( AES256(MK_<wallet_guid>, LP2) )

    // TODO: add encrypted wallet key to the account directory
    
    // create the wallet root directory if necessary
    ABC_CHECK_RET(ABC_WalletCreateRootDir(pError));
    
    // TODO: Create the wallet directory - <Wallet_UUID1>  <- All data in this directory encrypted with MK_<Wallet_UUID1>
    ABC_CHECK_RET(ABC_WalletGetDirName(&szWalletDir, szUUID, pError));
    ABC_CHECK_RET(ABC_FileIOCreateDir(szWalletDir, pError));

    // TODO: Create the Wallet_Name.json - wallet name

    // TODO: Create the Accounts.json - account names that have this wallet

    // TODO: Create the Private_Seed.json - bit coin seed

    // TODO: Create the BitCoin_Keys.json - bit coin keys

    // TODO: Create sync info for Wallet directory - .git
    
exit:
    if (szWalletDir)  free(szWalletDir);
    if (szAccountDir) free(szAccountDir);
    if (szUUID) free(szUUID);
    ABC_BUF_FREE(MK);

    return cc;
}

// creates the wallet directory if needed
static
tABC_CC ABC_WalletCreateRootDir(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char *szWalletRoot = NULL;
    
    // create the account directory string
    ABC_CHECK_RET(ABC_WalletGetRootDirName(&szWalletRoot, pError));
    
    // if it doesn't exist
    if (ABC_FileIOFileExist(szWalletRoot) != true)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szWalletRoot, pError));
    }
    
exit:
    if (szWalletRoot) free(szWalletRoot);
    
    return cc;
}

// gets the root directory for the wallets
// the string is allocated so it is up to the caller to free it
static
tABC_CC ABC_WalletGetRootDirName(char **pszRootDir, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    ABC_CHECK_NULL(pszRootDir);
    
    const char *szFileIORootDir;
    ABC_CHECK_RET(ABC_FileIOGetRootDir(&szFileIORootDir, pError));
    
    // create the wallet directory string
    *pszRootDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszRootDir, "%s/%s", szFileIORootDir, WALLET_DIR);
    
exit:
    
    return cc;
}

// gets the directory for the given wallet UUID
// the string is allocated so it is up to the caller to free it
static
tABC_CC ABC_WalletGetDirName(char **pszDir, char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    char *szWalletRootDir = NULL;
    
    ABC_CHECK_NULL(pszDir);
    
    ABC_CHECK_RET(ABC_WalletGetRootDirName(&szWalletRootDir, pError));
    
    // create the account directory string
    *pszDir = calloc(1, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszDir, "%s/%s", szWalletRootDir, szWalletUUID);
    
exit:
    if (szWalletRootDir) free(szWalletRootDir);
    
    return cc;
}
