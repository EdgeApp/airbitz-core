/**
 * @file
 * AirBitz Tx functions.
 *
 * This file contains all of the functions associated with transaction creation,
 * viewing and modification.
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>
#include <qrencode.h>
#include "ABC_Tx.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Crypto.h"
#include "ABC_Account.h"
#include "ABC_Mutex.h"
#include "ABC_Wallet.h"
#include "ABC_Debug.h"
#include "ABC_Bridge.h"

#define SATOSHI_PER_BITCOIN             100000000

#define CURRENCY_NUM_USD                840

#define TX_MAX_ADDR_ID_LENGTH           20 // largest char count for the string version of the id number - 20 digits should handle it

#define TX_MAX_AMOUNT_LENGTH            100 // should be max length of a bit coin amount string

#define TX_INTERNAL_SUFFIX              "-int.json" // the transaction was created by our direct action (i.e., send)
#define TX_EXTERNAL_SUFFIX              "-ext.json" // the transaction was created due to events in the block-chain (usually receives)

#define ADDRESS_FILENAME_SEPARATOR      '-'
#define ADDRESS_FILENAME_SUFFIX         ".json"
#define ADDRESS_FILENAME_MIN_LEN        8 // <id>-<public_addr>.json

#define JSON_DETAILS_FIELD              "meta"
#define JSON_CREATION_DATE_FIELD        "creationDate"
#define JSON_AMOUNT_SATOSHI_FIELD       "amountSatoshi"

#define JSON_TX_ID_FIELD                "ntxid"
#define JSON_TX_STATE_FIELD             "state"
#define JSON_TX_INTERNAL_FIELD          "internal"
#define JSON_TX_AMOUNT_CURRENCY_FIELD   "amountCurrency"
#define JSON_TX_NAME_FIELD              "name"
#define JSON_TX_CATEGORY_FIELD          "category"
#define JSON_TX_NOTES_FIELD             "notes"
#define JSON_TX_ATTRIBUTES_FIELD        "attributes"

#define JSON_ADDR_SEQ_FIELD             "seq"
#define JSON_ADDR_ADDRESS_FIELD         "address"
#define JSON_ADDR_STATE_FIELD           "state"
#define JSON_ADDR_RECYCLEABLE_FIELD     "recycleable"
#define JSON_ADDR_ACTIVITY_FIELD        "activity"
#define JSON_ADDR_DATE_FIELD            "date"

typedef enum eTxType
{
    TxType_None = 0,
    TxType_Internal,
    TxType_External
} tTxType;

typedef struct sTxStateInfo
{
    int64_t timeCreation;
    bool    bInternal;
} tTxStateInfo;

typedef struct sABC_Tx
{
    char            *szID; // ntxid from bitcoin
    tABC_TxDetails  *pDetails;
    tTxStateInfo    *pStateInfo;
} tABC_Tx;

typedef struct sTxAddressActivity
{
    char    *szTxID; // ntxid from bitcoin associated with this activity
    int64_t timeCreation;
    int64_t amountSatoshi;
} tTxAddressActivity;

typedef struct sTxAddressStateInfo
{
    int64_t             timeCreation;
    bool                bRecycleable;
    unsigned int        countActivities;
    tTxAddressActivity  *aActivities;
} tTxAddressStateInfo;

typedef struct sABC_TxAddress
{
    int32_t             seq; // sequence number
    char                *szID; // sequence number in string form
    char                *szPubAddress; // public address
    tABC_TxDetails      *pDetails;
    tTxAddressStateInfo *pStateInfo;
} tABC_TxAddress;

static tABC_CC  ABC_TxSend(tABC_TxSendInfo *pInfo, char **pszUUID, tABC_Error *pError);
static tABC_CC  ABC_TxCreateNewAddress(const char *szUserName, const char *szPassword, const char *szWalletUUID, tABC_TxDetails *pDetails, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_GetAddressFilename(const char *szWalletUUID, const char *szRequestID, char **pszFilename, tABC_Error *pError);
static tABC_CC  ABC_TxParseAddrFilename(const char *szFilename, char **pszID, char **pszPublicAddress, tABC_Error *pError);
static tABC_CC  ABC_TxSetAddressRecycle(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szAddress, bool bRecyclable, tABC_Error *pError);
static tABC_CC  ABC_TxCheckForInternalEquivalent(const char *szFilename, bool *pbEquivalent, tABC_Error *pError);
static tABC_CC  ABC_TxGetTxTypeAndBasename(const char *szFilename, tTxType *pType, char **pszBasename, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTxAndAppendToArray(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szFilename, tABC_TxInfo ***paTransactions, unsigned int *pCount, tABC_Error *pError);
static void     ABC_TxFreeTransaction(tABC_TxInfo *pTransactions);
static tABC_CC  ABC_TxGetAddressOwed(tABC_TxAddress *pAddr, int64_t *pSatoshiBalance, tABC_Error *pError);
static void     ABC_TxFreeRequest(tABC_RequestInfo *pRequest);
static tABC_CC  ABC_TxCreateTxFilename(char **pszFilename, const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szTxID, bool bInternal, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTransaction(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szFilename, tABC_Tx **ppTx, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeTxState(json_t *pJSON_Obj, tTxStateInfo **ppInfo, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails **ppDetails, tABC_Error *pError);
static void     ABC_TxFreeTx(tABC_Tx *pTx);
static tABC_CC  ABC_TxCreateTxDir(const char *szWalletUUID, tABC_Error *pError);
static tABC_CC  ABC_TxSaveTransaction(const char *szUserName, const char *szPassword, const char *szWalletUUID, const tABC_Tx *pTx, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeTxState(json_t *pJSON_Obj, tTxStateInfo *pInfo, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails *pDetails, tABC_Error *pError);
static int      ABC_TxInfoPtrCompare (const void * a, const void * b);
static tABC_CC  ABC_TxLoadAddress(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szAddressID, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_TxLoadAddressFile(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szFilename, tABC_TxAddress **ppAddress, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo **ppState, tABC_Error *pError);
static tABC_CC  ABC_TxSaveAddress(const char *szUserName, const char *szPassword, const char *szWalletUUID, const tABC_TxAddress *pAddress, tABC_Error *pError);
static tABC_CC  ABC_TxEncodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo *pInfo, tABC_Error *pError);
static tABC_CC  ABC_TxCreateAddressFilename(char **pszFilename, const char *szUserName, const char *szPassword, const char *szWalletUUID, const tABC_TxAddress *pAddress, tABC_Error *pError);
static tABC_CC  ABC_TxCreateAddressDir(const char *szWalletUUID, tABC_Error *pError);
static void     ABC_TxFreeAddress(tABC_TxAddress *pAddress);
static void     ABC_TxFreeAddressStateInfo(tTxAddressStateInfo *pInfo);
static void     ABC_TxFreeAddresses(tABC_TxAddress **aAddresses, unsigned int count);
static tABC_CC  ABC_TxGetAddresses(const char *szUserName, const char *szPassword, const char *szWalletUUID, tABC_TxAddress ***paAddresses, unsigned int *pCount, tABC_Error *pError);
static int      ABC_TxAddrPtrCompare(const void * a, const void * b);
static tABC_CC  ABC_TxLoadAddressAndAppendToArray(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szFilename, tABC_TxAddress ***paAddresses, unsigned int *pCount, tABC_Error *pError);
//static void     ABC_TxPrintAddresses(tABC_TxAddress **aAddresses, unsigned int count);
static tABC_CC  ABC_TxMutexLock(tABC_Error *pError);
static tABC_CC  ABC_TxMutexUnlock(tABC_Error *pError);

/**
 * Allocate a send info struct and populate it with the data given
 */
tABC_CC ABC_TxSendInfoAlloc(tABC_TxSendInfo **ppTxSendInfo,
                            const char *szUserName,
                            const char *szPassword,
                            const char *szWalletUUID,
                            const char *szDestAddress,
                            const tABC_TxDetails *pDetails,
                            tABC_Request_Callback fRequestCallback,
                            void *pData,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxSendInfo *pTxSendInfo = NULL;

    ABC_CHECK_NULL(ppTxSendInfo);
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_NULL(szDestAddress);
    ABC_CHECK_NULL(pDetails);
    ABC_CHECK_NULL(fRequestCallback);

    ABC_ALLOC(pTxSendInfo, sizeof(tABC_TxSendInfo));

    ABC_STRDUP(pTxSendInfo->szUserName, szUserName);
    ABC_STRDUP(pTxSendInfo->szPassword, szPassword);
    ABC_STRDUP(pTxSendInfo->szWalletUUID, szWalletUUID);
    ABC_STRDUP(pTxSendInfo->szDestAddress, szDestAddress);

    ABC_CHECK_RET(ABC_TxDupDetails(&(pTxSendInfo->pDetails), pDetails, pError));

    pTxSendInfo->fRequestCallback = fRequestCallback;

    pTxSendInfo->pData = pData;

    *ppTxSendInfo = pTxSendInfo;
    pTxSendInfo = NULL;

exit:
    ABC_TxSendInfoFree(pTxSendInfo);

    return cc;
}

/**
 * Free a send info struct
 */
void ABC_TxSendInfoFree(tABC_TxSendInfo *pTxSendInfo)
{
    if (pTxSendInfo)
    {
        ABC_FREE_STR(pTxSendInfo->szUserName);
        ABC_FREE_STR(pTxSendInfo->szPassword);
        ABC_FREE_STR(pTxSendInfo->szWalletUUID);
        ABC_FREE_STR(pTxSendInfo->szDestAddress);

        ABC_TxFreeDetails(pTxSendInfo->pDetails);

        ABC_CLEAR_FREE(pTxSendInfo, sizeof(tABC_TxSendInfo));
    }
}

/**
 * Sends a transaction. Assumes it is running in a thread.
 *
 * This function sends a transaction.
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to send a transaction (should be a tABC_TxSendInfo)
 */
void *ABC_TxSendThreaded(void *pData)
{
    tABC_TxSendInfo *pInfo = (tABC_TxSendInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;
        memset(&results, 0, sizeof(tABC_RequestResults));

        results.requestType = ABC_RequestType_SendBitcoin;

        results.bSuccess = false;

        // send the transaction
        tABC_CC CC = ABC_TxSend(pInfo, (char **) &(results.pRetData), &(results.errorInfo));
        results.errorInfo.code = CC;

        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);

        // it is our responsibility to free the info struct
        ABC_TxSendInfoFree(pInfo);
    }

    return NULL;
}

/**
 * Sends the transaction with the given info.
 *
 * @param pInfo Pointer to transaction information
 * @param pszTxID Pointer to hold allocated pointer to transaction ID string
 */
static
tABC_CC ABC_TxSend(tABC_TxSendInfo  *pInfo,
                   char             **pszTxID,
                   tABC_Error       *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(pInfo);
    ABC_CHECK_NULL(pszTxID);

    // temp: just create the transaction id
    ABC_STRDUP(*pszTxID, "ID");

    // TODO: write the real function -
    /*
     creates and address with the given info and sends transaction to block chain
     */
    /*
     Core creates an address with the given info and sends transaction to block chain:
     1) Ask libwallet for data required to send the amount
        input:
            amount
            list of source addresses
        output:
            fundable (do we have the funds),
            source addresses used,
                change address needed (any change?),
        fee
     2) If there is need for a ‘change address’, look for an address with the recycle bit set, if none found, ask libwallet to create a Bitcoin_Address (for change) using N+1 where N is the current number of addresses.
     3) Ask libwallet to send the amount to the address
        input: dest address, source address list (provided in step 1), amount and change Bitcoin_Address (if needed).
        output: TX ID.
     4) If there is a change address, create an Address Entry with the Bitcoin_Address and the send funds meta data (e.g., name, amount, etc).
     5) Create a transaction in the transaction table with meta data using the TX ID from step 3.
     */

exit:

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Duplicate a TX details struct
 */
tABC_CC ABC_TxDupDetails(tABC_TxDetails **ppNewDetails, const tABC_TxDetails *pOldDetails, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxDetails *pNewDetails = NULL;

    ABC_CHECK_NULL(ppNewDetails);
    ABC_CHECK_NULL(pOldDetails);

    ABC_ALLOC(pNewDetails, sizeof(tABC_TxDetails));

    pNewDetails->amountSatoshi  = pOldDetails->amountSatoshi;
    pNewDetails->amountCurrency  = pOldDetails->amountCurrency;
    pNewDetails->attributes = pOldDetails->attributes;
    if (pOldDetails->szName != NULL)
    {
        ABC_STRDUP(pNewDetails->szName, pOldDetails->szName);
    }
    if (pOldDetails->szCategory != NULL)
    {
        ABC_STRDUP(pNewDetails->szCategory, pOldDetails->szCategory);
    }
    if (pOldDetails->szNotes != NULL)
    {
        ABC_STRDUP(pNewDetails->szNotes, pOldDetails->szNotes);
    }

    // set the pointer for the caller
    *ppNewDetails = pNewDetails;
    pNewDetails = NULL;

exit:
    ABC_TxFreeDetails(pNewDetails);

    return cc;
}

/**
 * Free a TX details struct
 */
void ABC_TxFreeDetails(tABC_TxDetails *pDetails)
{
    if (pDetails)
    {
        ABC_FREE_STR(pDetails->szName);
        ABC_FREE_STR(pDetails->szCategory);
        ABC_FREE_STR(pDetails->szNotes);
        ABC_CLEAR_FREE(pDetails, sizeof(tABC_TxDetails));
    }
}

/**
 * Converts amount from Satoshi to Bitcoin
 *
 * @param satoshi Amount in Satoshi
 */
double ABC_TxSatoshiToBitcoin(int64_t satoshi)
{
    return((double) satoshi / (double) SATOSHI_PER_BITCOIN);
}

/**
 * Converts amount from Bitcoin to Satoshi
 *
 * @param bitcoin Amount in Bitcoin
 */
int64_t ABC_TxBitcoinToSatoshi(double bitcoin)
{
    return((int64_t) (bitcoin * (double) SATOSHI_PER_BITCOIN));
}

/**
 * Converts Satoshi to given currency
 *
 * @param satoshi     Amount in Satoshi
 * @param pCurrency   Pointer to location to store amount converted to currency.
 * @param currencyNum Currency ISO 4217 num
 * @param pError      A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxSatoshiToCurrency(int64_t satoshi,
                              double *pCurrency,
                              int currencyNum,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(pCurrency);
    *pCurrency = 0.0;

    // currently only supporting dollars
    if (CURRENCY_NUM_USD == currencyNum)
    {
        // TODO: find real conversion - for now hardcode to $600 per bitcoin
        *pCurrency = ABC_SatoshiToBitcoin(satoshi) * 600;
    }
    else
    {
        ABC_RET_ERROR(ABC_CC_NotSupported, "The given currency is not currently supported");
    }

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
tABC_CC ABC_TxCurrencyToSatoshi(double currency,
                              int currencyNum,
                              int64_t *pSatoshi,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(pSatoshi);
    *pSatoshi = 0;

    // currently only supporting dollars
    if (CURRENCY_NUM_USD == currencyNum)
    {
        // TODO: find real conversion - for now hardcode to $600 per bitcoin
        *pSatoshi = ABC_BitcoinToSatoshi(currency) * 600;
    }
    else
    {
        ABC_RET_ERROR(ABC_CC_NotSupported, "The given currency is not currently supported");
    }

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
tABC_CC ABC_TxCreateReceiveRequest(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 tABC_TxDetails *pDetails,
                                 char **pszRequestID,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxAddress *pAddress = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(pDetails);
    ABC_CHECK_NULL(pszRequestID);
    *pszRequestID = NULL;

    // get a new address (re-using a recycleable if we can)
    ABC_CHECK_RET(ABC_TxCreateNewAddress(szUserName, szPassword, szWalletUUID, pDetails, &pAddress, pError));

    // save out this address
    ABC_CHECK_RET(ABC_TxSaveAddress(szUserName, szPassword, szWalletUUID, pAddress, pError));

    // set the id for the caller
    ABC_STRDUP(*pszRequestID, pAddress->szID);

exit:
    ABC_TxFreeAddress(pAddress);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Creates a new address.
 * First looks to see if we can recycle one, if we can, that is the address returned.
 * This new address is not saved to the file system, the caller must make sure it is saved
 * if they want it persisted.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param pDetails      Pointer to transaction details to be used for the new address
 *                      (note: a copy of these are made so the caller can do whatever they want
 *                       with the pointer once the call is complete)
 * @param ppAddress     Location to store pointer to allocated address
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxCreateNewAddress(const char *szUserName,
                               const char *szPassword,
                               const char *szWalletUUID,
                               tABC_TxDetails *pDetails,
                               tABC_TxAddress **ppAddress,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxAddress **aAddresses = NULL;
    unsigned int countAddresses = 0;
    tABC_TxAddress *pAddress = NULL;
    int64_t N = -1;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(pDetails);
    ABC_CHECK_NULL(ppAddress);

    // first look for an existing address that we can re-use

    // load addresses
    ABC_CHECK_RET(ABC_TxGetAddresses(szUserName, szPassword, szWalletUUID, &aAddresses, &countAddresses, pError));

    // search through all of the addresses, get the highest N and check for one with the recycleable bit set
    for (int i = 0; i < countAddresses; i++)
    {
        // if we don't have an address yet and this one is available
        ABC_CHECK_NULL(aAddresses[i]->pStateInfo);
        if ((pAddress == NULL) && (aAddresses[i]->pStateInfo->bRecycleable == true) && ((aAddresses[i]->pStateInfo->countActivities == 0)))
        {
            pAddress = aAddresses[i];
            // set it to NULL so we don't free it as part of the array free, we will be sending this back to the caller
            aAddresses[i] = NULL;
        }

        // if this is the highest seq number
        if (aAddresses[i]->seq > N)
        {
            N = aAddresses[i]->seq;
        }
    }

    // if we found an address, make it ours!
    if (pAddress != NULL)
    {
        // free state and details as we will be setting them to new data below
        ABC_TxFreeAddressStateInfo(pAddress->pStateInfo);
        pAddress->pStateInfo = NULL;
        ABC_FreeTxDetails(pAddress->pDetails);
        pAddress->pDetails = NULL;
    }
    else // if we didn't find an address, we need to create a new one
    {
        ABC_ALLOC(pAddress, sizeof(tABC_TxAddress));

        // get the private seed so we can generate the public address
        tABC_U08Buf Seed = ABC_BUF_NULL;
        ABC_CHECK_RET(ABC_WalletGetBitcoinPrivateSeed(szUserName, szPassword, szWalletUUID, &Seed, pError));

        // generate the public address
        pAddress->szPubAddress = NULL;
        pAddress->seq = (int32_t) N;
        do
        {
            // move to the next sequence number
            pAddress->seq++;

            // Get the public address for our sequence (it can return NULL, if it is invalid)
            ABC_CHECK_RET(ABC_BridgeGetBitcoinPubAddress(&(pAddress->szPubAddress), Seed, pAddress->seq, pError));
        } while (pAddress->szPubAddress == NULL);

        // set the final ID
        ABC_ALLOC(pAddress->szID, TX_MAX_ADDR_ID_LENGTH);
        sprintf(pAddress->szID, "%u", pAddress->seq);
    }

    // add the info we have to this address

    // copy over the info we were given
    ABC_CHECK_RET(ABC_DuplicateTxDetails(&(pAddress->pDetails), pDetails, pError));

    // create the state info
    ABC_ALLOC(pAddress->pStateInfo, sizeof(tTxAddressStateInfo));
    pAddress->pStateInfo->bRecycleable = true;
    pAddress->pStateInfo->countActivities = 0;
    pAddress->pStateInfo->aActivities = NULL;
    pAddress->pStateInfo->timeCreation = time(NULL);

    // assigned final address
    *ppAddress = pAddress;
    pAddress = NULL;

exit:
    ABC_TxFreeAddresses(aAddresses, countAddresses);
    ABC_TxFreeAddress(pAddress);
    
    ABC_TxMutexUnlock(NULL);
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
tABC_CC ABC_TxModifyReceiveRequest(const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   const char *szRequestID,
                                   tABC_TxDetails *pDetails,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFile = NULL;
    char *szAddrDir = NULL;
    char *szFilename = NULL;
    tABC_TxAddress *pAddress = NULL;
    tABC_TxDetails *pNewDetails = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");
    ABC_CHECK_NULL(pDetails);

    // get the filename for this request (note: interally requests are addresses)
    ABC_CHECK_RET(ABC_GetAddressFilename(szWalletUUID, szRequestID, &szFile, pError));

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // create the full filename
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH + 1);
    sprintf(szFilename, "%s/%s", szAddrDir, szFile);

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(szUserName, szPassword, szWalletUUID, szFilename, &pAddress, pError));

    // copy the new details
    ABC_CHECK_RET(ABC_TxDupDetails(&pNewDetails, pDetails, pError));

    // free the old details on this address
    ABC_TxFreeDetails(pAddress->pDetails);

    // set the new details
    pAddress->pDetails = pNewDetails;
    pNewDetails = NULL;

    // write out the address
    ABC_CHECK_RET(ABC_TxSaveAddress(szUserName, szPassword, szWalletUUID, pAddress, pError));

exit:
    ABC_FREE_STR(szFile);
    ABC_FREE_STR(szAddrDir);
    ABC_FREE_STR(szFilename);
    ABC_TxFreeAddress(pAddress);
    ABC_TxFreeDetails(pNewDetails);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Gets the filename for a given address based upon the address id
 *
 * @param szWalletUUID  UUID of the wallet associated with this address
 * @param szAddressID   ID of this address
 * @param pszFilename   Address to store pointer to filename
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_GetAddressFilename(const char *szWalletUUID,
                               const char *szAddressID,
                               char **pszFilename,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szAddrDir = NULL;
    tABC_FileIOList *pFileList = NULL;

    char *szID = NULL;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError)); // we want this as an atomic files system function
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szAddressID);
    ABC_CHECK_ASSERT(strlen(szAddressID) > 0, ABC_CC_Error, "No address UUID provided");
    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // Make sure there is an addresses directory
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szAddrDir, &bExists, pError));
    ABC_CHECK_ASSERT(bExists == true, ABC_CC_Error, "No existing requests/addresses");

    // get all the files in the address directory
    ABC_FileIOCreateFileList(&pFileList, szAddrDir, NULL);
    for (int i = 0; i < pFileList->nCount; i++)
    {
        // if this file is a normal file
        if (pFileList->apFiles[i]->type == ABC_FileIOFileType_Regular)
        {
            // parse the elements from the filename
            ABC_FREE_STR(szID);
            ABC_CHECK_RET(ABC_TxParseAddrFilename(pFileList->apFiles[i]->szName, &szID, NULL, pError));

            // if the id matches
            if (strcmp(szID, szAddressID) == 0)
            {
                // copy over the filename
                ABC_STRDUP(*pszFilename, pFileList->apFiles[i]->szName);
                break;
            }
        }
    }

exit:
    ABC_FREE_STR(szAddrDir);
    ABC_FREE_STR(szID);
    ABC_FileIOFreeFileList(pFileList);

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

/**
 * Parses out the id and public address from an address filename
 *
 * @param szFilename        Filename to parse
 * @param pszID             Location to store allocated id (caller must free) - optional
 * @param pszPublicAddress  Location to store allocated public address(caller must free) - optional
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxParseAddrFilename(const char *szFilename,
                                char **pszID,
                                char **pszPublicAddress,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    // if the filename is long enough
    if (strlen(szFilename) >= ADDRESS_FILENAME_MIN_LEN)
    {
        int suffixPos = (int) strlen(szFilename) - (int) strlen(ADDRESS_FILENAME_SUFFIX);
        char *szSuffix = (char *) &(szFilename[suffixPos]);

        // if the filename ends with the right suffix
        if (strcmp(ADDRESS_FILENAME_SUFFIX, szSuffix) == 0)
        {
            int nPosSeparator = 0;

            // go through all the characters up to the separator
            for (int i = 0; i < strlen(szFilename); i++)
            {
                // check for id separator
                if (szFilename[i] == ADDRESS_FILENAME_SEPARATOR)
                {
                    // found it
                    nPosSeparator = i;
                    break;
                }

                // if no separator, then better be a digit
                if (isdigit(szFilename[i]) == 0)
                {
                    // Ran into a non-digit! - no good
                    break;
                }
            }

            // if we found a legal separator position
            if (nPosSeparator > 0)
            {
                if (pszID != NULL)
                {
                    ABC_ALLOC(*pszID, nPosSeparator + 1);
                    strncpy(*pszID, szFilename, nPosSeparator);
                }

                if (pszPublicAddress != NULL)
                {
                    ABC_STRDUP(*pszPublicAddress, &(szFilename[nPosSeparator + 1]))
                    (*pszPublicAddress)[strlen(*pszPublicAddress) - strlen(ADDRESS_FILENAME_SUFFIX)] = '\0';
                }
            }
        }
    }

exit:

    return cc;
}

/**
 * Finalizes a previously created receive request.
 * This is done by setting the recycle bit to false so that the address is not used again.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxFinalizeReceiveRequest(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     const char *szRequestID,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");

    // set the recycle bool to false (not that the request is actually an address internally)
    ABC_CHECK_RET(ABC_TxSetAddressRecycle(szUserName, szPassword, szWalletUUID, szRequestID, false, pError));

exit:

    return cc;
}

/**
 * Cancels a previously created receive request.
 * This is done by setting the recycle bit to true so that the address can be used again.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxCancelReceiveRequest(const char *szUserName,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   const char *szRequestID,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");

    // set the recycle bool to true (not that the request is actually an address internally)
    ABC_CHECK_RET(ABC_TxSetAddressRecycle(szUserName, szPassword, szWalletUUID, szRequestID, true, pError));

exit:

    return cc;
}

/**
 * Sets the recycle status on an address as specified
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szAddress     ID of the address
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxSetAddressRecycle(const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                const char *szAddress,
                                bool bRecyclable,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFile = NULL;
    char *szAddrDir = NULL;
    char *szFilename = NULL;
    tABC_TxAddress *pAddress = NULL;
    tABC_TxDetails *pNewDetails = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szAddress);
    ABC_CHECK_ASSERT(strlen(szAddress) > 0, ABC_CC_Error, "No address ID provided");

    // get the filename for this address
    ABC_CHECK_RET(ABC_GetAddressFilename(szWalletUUID, szAddress, &szFile, pError));

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // create the full filename
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH + 1);
    sprintf(szFilename, "%s/%s", szAddrDir, szFile);

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(szUserName, szPassword, szWalletUUID, szFilename, &pAddress, pError));
    ABC_CHECK_NULL(pAddress->pStateInfo);

    // if it isn't already set as required
    if (pAddress->pStateInfo->bRecycleable != bRecyclable)
    {
        // change the recycle boolean
        ABC_CHECK_NULL(pAddress->pStateInfo);
        pAddress->pStateInfo->bRecycleable = bRecyclable;

        // write out the address
        ABC_CHECK_RET(ABC_TxSaveAddress(szUserName, szPassword, szWalletUUID, pAddress, pError));
    }

exit:
    ABC_FREE_STR(szFile);
    ABC_FREE_STR(szAddrDir);
    ABC_FREE_STR(szFilename);
    ABC_TxFreeAddress(pAddress);
    ABC_TxFreeDetails(pNewDetails);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Generate the QR code for a previously created receive request.
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szRequestID   ID of this request
 * @param paData        Pointer to store array of data bytes (0x0 white, 0x1 black)
 * @param pWidth        Pointer to store width of image (image will be square)
 * @param pError        A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGenerateRequestQRCode(const char *szUserName,
                                    const char *szPassword,
                                    const char *szWalletUUID,
                                    const char *szRequestID,
                                    unsigned char **paData,
                                    unsigned int *pWidth,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxAddress *pAddress = NULL;
    tABC_U08Buf StringData = ABC_BUF_NULL;
    QRcode *qr = NULL;
    unsigned char *aData = NULL;
    unsigned int length = 0;
    char *szURI = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");

    // load the request/address
    ABC_CHECK_RET(ABC_TxLoadAddress(szUserName, szPassword, szWalletUUID, szRequestID, &pAddress, pError));
    ABC_CHECK_NULL(pAddress->pDetails);

    // TODO: Call when available in bridge
    /*
    tABC_BitcoinURIInfo infoURI;
    memset(&infoURI, 0, sizeof(tABC_BitcoinURIInfo));
    infoURI.amountSatoshi = pAddress->pDetails->amountSatoshi;
    infoURI.szAddress = pAddress->szPubAddress;
    // if there is a name
    if (pAddress->pDetails->szName)
    {
        if (strlen(pAddress->pDetails->szName) > 0)
        {
            infoURI.szLabel = pAddress->pDetails->szName;
        }
    }

    // if there is a note
    if (pAddress->pDetails->szNotes)
    {
        if (strlen(pAddress->pDetails->szNotes) > 0)
        {
            infoURI.szMessage = pAddress->pDetails->szNotes;
        }
    }
    ABC_CHECK_RET(ABC_BridgeEncodeBitcoinURI(&szURI, &infoURI, pError));
     */

    /////////////////////////////////////////////
    // start temp until we move to Bridge
    ////////////////////////////////////////////

    // start with the bitcoin label
    ABC_BUF_DUP_PTR(StringData, "bitcoin:", strlen("bitcoin:"));

    // added the address
    ABC_BUF_APPEND_PTR(StringData, pAddress->szPubAddress, strlen(pAddress->szPubAddress));

    // add the amount
    static char szAmount[TX_MAX_AMOUNT_LENGTH];
    sprintf(szAmount, "?amount=%lld.%08lld", pAddress->pDetails->amountSatoshi / SATOSHI_PER_BITCOIN, pAddress->pDetails->amountSatoshi % SATOSHI_PER_BITCOIN);
    ABC_BUF_APPEND_PTR(StringData, szAmount, strlen(szAmount));

    // if there is a name
    if (pAddress->pDetails->szName)
    {
        if (strlen(pAddress->pDetails->szName) > 0)
        {
            ABC_BUF_APPEND_PTR(StringData, "&label=", strlen("&label="));
            ABC_BUF_APPEND_PTR(StringData, pAddress->pDetails->szName, strlen(pAddress->pDetails->szName));
        }
    }

    // if there is a note
    if (pAddress->pDetails->szNotes)
    {
        if (strlen(pAddress->pDetails->szNotes) > 0)
        {
            ABC_BUF_APPEND_PTR(StringData, "&message=", strlen("&message="));
            ABC_BUF_APPEND_PTR(StringData, pAddress->pDetails->szNotes, strlen(pAddress->pDetails->szNotes));
        }
    }

    // add the final null so we get a true string
    ABC_BUF_APPEND_PTR(StringData, "", 1);
    szURI = (char *) ABC_BUF_PTR(StringData);

    /////////////////////////////////////////////
    // end temp until we move to Bridge
    ////////////////////////////////////////////

    // encode our string
    ABC_DebugLog("Encoding: %s", szURI);
    qr = QRcode_encodeString(szURI, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
    ABC_CHECK_ASSERT(qr != NULL, ABC_CC_Error, "Unable to create QR code");
    length = qr->width * qr->width;
    ABC_ALLOC(aData, length);
    for (int i = 0; i < length; i++)
    {
        aData[i] = qr->data[i] & 0x1;
    }
    *pWidth = qr->width;
    *paData = aData;
    aData = NULL;

exit:
    ABC_TxFreeAddress(pAddress);
    ABC_FREE_STR(szURI);
    QRcode_free(qr);
    ABC_CLEAR_FREE(aData, length);

    ABC_TxMutexUnlock(NULL);
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
tABC_CC ABC_TxGetTransactions(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              tABC_TxInfo ***paTransactions,
                              unsigned int *pCount,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szTxDir = NULL;
    tABC_FileIOList *pFileList = NULL;
    char *szFilename = NULL;
    tABC_TxInfo **aTransactions = NULL;
    unsigned int count = 0;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError)); // we want this as an atomic files system function
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(paTransactions);
    *paTransactions = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // validate the creditials
    ABC_CHECK_RET(ABC_WalletCheckCredentials(szUserName, szPassword, szWalletUUID, pError));

#if 0 // TODO: we can take this out once we are creating real transactions
    // start temp - create a transaction
    tABC_Tx Tx;
    char szID[] = "4ba1345764a1a704b97d062b47b55c8632efc4d7c0c8039d78adf8a52bcba4fe";
    Tx.szID = szID;
    tABC_TxDetails Details;
    Details.amountSatoshi = 55;
    Details.amountCurrency = 9.9;
    Details.szName = "My Name1";
    Details.szCategory = "My Category1";
    Details.szNotes = "My Notes1";
    Details.attributes = 0x1;
    Tx.pDetails = &Details;
    tTxStateInfo State;
    State.timeCreation = 123;
    State.bInternal = true;
    Tx.pStateInfo = &State;
    ABC_CHECK_RET(ABC_TxSaveTransaction(szUserName, szPassword, szWalletUUID, &Tx, pError));
    // end temp
#endif

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetTxDirName(&szTxDir, szWalletUUID, pError));

    // if there is a transaction directory
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szTxDir, &bExists, pError));

    if (bExists == true)
    {
        ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH + 1);

        // get all the files in the transaction directory
        ABC_FileIOCreateFileList(&pFileList, szTxDir, NULL);
        for (int i = 0; i < pFileList->nCount; i++)
        {
            // if this file is a normal file
            if (pFileList->apFiles[i]->type == ABC_FileIOFileType_Regular)
            {
                // create the filename for this transaction
                sprintf(szFilename, "%s/%s", szTxDir, pFileList->apFiles[i]->szName);

                // get the transaction type
                tTxType type = TxType_None;
                ABC_CHECK_RET(ABC_TxGetTxTypeAndBasename(szFilename, &type, NULL, pError));

                // if this is a transaction file (based upon name)
                if (type != TxType_None)
                {
                    bool bHasInternalEquivalent = false;

                    // if this is an external transaction
                    if (type == TxType_External)
                    {
                        // check if it has an internal equivalent and, if so, delete the external
                        ABC_CHECK_RET(ABC_TxCheckForInternalEquivalent(szFilename, &bHasInternalEquivalent, pError));
                    }

                    // if this doesn't not have an internal equivalent (or is an internal itself)
                    if (bHasInternalEquivalent == false)
                    {
                        // add this transaction to the array
                        ABC_CHECK_RET(ABC_TxLoadTxAndAppendToArray(szUserName, szPassword, szWalletUUID, szFilename, &aTransactions, &count, pError));
                    }
                }
            }
        }
    }

    // if we have more than one, then let's sort them
    if (count > 1)
    {
        // sort the transactions by creation date using qsort
        qsort(aTransactions, count, sizeof(tABC_TxInfo *), ABC_TxInfoPtrCompare);
    }

    // store final results
    *paTransactions = aTransactions;
    aTransactions = NULL;
    *pCount = count;
    count = 0;

exit:
    ABC_FREE_STR(szTxDir);
    ABC_FREE_STR(szFilename);
    ABC_FileIOFreeFileList(pFileList);
    ABC_TxFreeTransactions(aTransactions, count);

    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

/**
 * Looks to see if a matching internal (i.e., -int) version of this file exists.
 * If it does, this external version is deleted.
 *
 * @param szFilename    Filename of transaction
 * @param pbEquivalent  Pointer to store result
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxCheckForInternalEquivalent(const char *szFilename,
                                         bool *pbEquivalent,
                                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szBasename = NULL;
    char *szFilenameInt = NULL;
    tTxType type = TxType_None;

    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(pbEquivalent);
    *pbEquivalent = false;

    // get the type and the basename of this transaction
    ABC_CHECK_RET(ABC_TxGetTxTypeAndBasename(szFilename, &type, &szBasename, pError));

    // if this is an external
    if (type == TxType_External)
    {
        // create the internal version of the filename
        ABC_ALLOC(szFilenameInt, ABC_FILEIO_MAX_PATH_LENGTH + 1);
        sprintf(szFilenameInt, "%s%s", szBasename, TX_INTERNAL_SUFFIX);

        // check if this internal version of the file exists
        bool bExists = false;
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilenameInt, &bExists, pError));

        // if the internal version exists
        if (bExists)
        {
            // delete the external version (this one)
            ABC_CHECK_RET(ABC_FileIODeleteFile(szFilename, pError));

            *pbEquivalent = true;
        }
    }

exit:
    ABC_FREE_STR(szBasename);
    ABC_FREE_STR(szFilenameInt);

    return cc;
}

/**
 * Given a potential transaction filename, determines the type and 
 * creates an allocated basename if it is a transaction type.
 *
 * @param szFilename    Filename of potential transaction
 * @param pType         Pointer to store type
 * @param pszBasename   Pointer to store allocated basename (optional)
 *                      (caller must free)
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxGetTxTypeAndBasename(const char *szFilename,
                                   tTxType *pType,
                                   char **pszBasename,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szBasename = NULL;

    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(pType);

    // assume nothing found
    *pType = TxType_None;
    if (pszBasename != NULL)
    {
        *pszBasename = NULL;
    }

    // look for external the suffix
    int sizeSuffix = strlen(TX_EXTERNAL_SUFFIX);
    if (strlen(szFilename) > sizeSuffix)
    {
        char *szSuffix = (char *) szFilename + (strlen(szFilename) - sizeSuffix);

        // if this file ends with the external suffix
        if (strcmp(szSuffix, TX_EXTERNAL_SUFFIX) == 0)
        {
            *pType = TxType_External;

            // if they want the basename
            if (pszBasename != NULL)
            {
                ABC_STRDUP(szBasename, szFilename);
                szBasename[strlen(szFilename) - sizeSuffix] = '\0';
            }
        }
    }

    // if we haven't found it yet
    if (TxType_None == *pType)
    {
        // check for the internal
        sizeSuffix = strlen(TX_INTERNAL_SUFFIX);
        if (strlen(szFilename) > sizeSuffix)
        {
            char *szSuffix = (char *) szFilename + (strlen(szFilename) - sizeSuffix);

            // if this file ends with the external suffix
            if (strcmp(szSuffix, TX_INTERNAL_SUFFIX) == 0)
            {
                *pType = TxType_Internal;

                // if they want the basename
                if (pszBasename != NULL)
                {
                    ABC_STRDUP(szBasename, szFilename);
                    szBasename[strlen(szFilename) - sizeSuffix] = '\0';
                }
            }
        }
    }

    if (pszBasename != NULL)
    {
        *pszBasename = szBasename;
    }
    szBasename = NULL;
    
exit:
    ABC_FREE_STR(szBasename);
    
    return cc;
}

/**
 * Loads the given transaction and adds it to the end of the array
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szFilename        Filename of transaction
 * @param paTransactions    Pointer to array into which the transaction will be added
 * @param pCount            Pointer to store number of transactions (will be updated)
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadTxAndAppendToArray(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     const char *szFilename,
                                     tABC_TxInfo ***paTransactions,
                                     unsigned int *pCount,
                                     tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_Tx *pTx = NULL;
    tABC_TxInfo *pTransaction = NULL;
    tABC_TxInfo **aTransactions = NULL;
    unsigned int count = 0;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No transaction filename provided");
    ABC_CHECK_NULL(paTransactions);
    ABC_CHECK_NULL(pCount);

    // hold on to current values
    count = *pCount;
    aTransactions = *paTransactions;

    // load it into the internal transaction structure
    ABC_CHECK_RET(ABC_TxLoadTransaction(szUserName, szPassword, szWalletUUID, szFilename, &pTx, pError));

    // allocated the info version
    ABC_ALLOC(pTransaction, sizeof(tABC_TxInfo));

    // reassign the data over to the info struct
    pTransaction->szID = pTx->szID;
    pTx->szID = NULL;
    pTransaction->pDetails = pTx->pDetails;
    pTx->pDetails = NULL;
    ABC_CHECK_NULL(pTx->pStateInfo);
    pTransaction->timeCreation = pTx->pStateInfo->timeCreation;

    // create space for new entry
    if (aTransactions == NULL)
    {
        ABC_ALLOC(aTransactions, sizeof(tABC_TxInfo *));
        count = 1;
    }
    else
    {
        count++;
        ABC_REALLOC(aTransactions, sizeof(tABC_TxInfo *) * count);
    }

    // add it to the array
    aTransactions[count - 1] = pTransaction;
    pTransaction = NULL;

    // assign the values to the caller
    *paTransactions = aTransactions;
    *pCount = count;
    
exit:
    ABC_TxFreeTx(pTx);
    ABC_TxFreeTransaction(pTransaction);

    return cc;
}

/**
 * Frees the given transaction
 *
 * @param pTransaction Pointer to transaction to free
 */
static
void ABC_TxFreeTransaction(tABC_TxInfo *pTransaction)
{
    if (pTransaction)
    {
        ABC_FREE_STR(pTransaction->szID);

        ABC_TxFreeDetails(pTransaction->pDetails);

        ABC_CLEAR_FREE(pTransaction, sizeof(tABC_TxInfo));
    }
}

/**
 * Frees the given array of transactions
 *
 * @param aTransactions Array of transactions
 * @param count         Number of transactions
 */
void ABC_TxFreeTransactions(tABC_TxInfo **aTransactions,
                            unsigned int count)
{
    if (aTransactions && count > 0)
    {
        for (int i = 0; i < count; i++)
        {
            ABC_TxFreeTransaction(aTransactions[i]);
        }

        ABC_FREE(aTransactions);
    }
}

/**
 * Sets the details for a specific existing transaction.
 *
 * @param szUserName        UserName for the account associated with the transaction
 * @param szPassword        Password for the account associated with the transaction
 * @param szWalletUUID      UUID of the wallet associated with the transaction
 * @param szID              ID of the transaction
 * @param pDetails          Details for the transaction
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxSetTransactionDetails(const char *szUserName,
                                    const char *szPassword,
                                    const char *szWalletUUID,
                                    const char *szID,
                                    tABC_TxDetails *pDetails,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFilename = NULL;
    tABC_Tx *pTx = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szID);
    ABC_CHECK_ASSERT(strlen(szID) > 0, ABC_CC_Error, "No transaction ID provided");

    // validate the creditials
    ABC_CHECK_RET(ABC_WalletCheckCredentials(szUserName, szPassword, szWalletUUID, pError));

    // find the filename of the existing transaction

    // first try the internal
    ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, szID, true, pError));
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));

    // if the internal doesn't exist
    if (bExists == false)
    {
        // try the external
        ABC_FREE_STR(szFilename);
        ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, szID, false, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    }

    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

    // load the existing transaction
    ABC_CHECK_RET(ABC_TxLoadTransaction(szUserName, szPassword, szWalletUUID, szFilename, &pTx, pError));
    ABC_CHECK_NULL(pTx->pDetails);
    ABC_CHECK_NULL(pTx->pStateInfo);

    // modify the details
    pTx->pDetails->amountSatoshi = pDetails->amountSatoshi;
    pTx->pDetails->amountCurrency = pDetails->amountCurrency;
    pTx->pDetails->attributes = pDetails->attributes;
    ABC_FREE_STR(pTx->pDetails->szName);
    ABC_STRDUP(pTx->pDetails->szName, pDetails->szName);
    ABC_FREE_STR(pTx->pDetails->szCategory);
    ABC_STRDUP(pTx->pDetails->szCategory, pDetails->szCategory);
    ABC_FREE_STR(pTx->pDetails->szNotes);
    ABC_STRDUP(pTx->pDetails->szNotes, pDetails->szNotes);

    // re-save the transaction
    ABC_CHECK_RET(ABC_TxSaveTransaction(szUserName, szPassword, szWalletUUID, pTx, pError));

exit:
    ABC_FREE_STR(szFilename);
    ABC_TxFreeTx(pTx);

    ABC_TxMutexUnlock(NULL);
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
tABC_CC ABC_TxGetPendingRequests(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 tABC_RequestInfo ***paRequests,
                                 unsigned int *pCount,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxAddress **aAddresses = NULL;
    unsigned int count = 0;
    tABC_RequestInfo **aRequests = NULL;
    unsigned int countPending = 0;
    tABC_RequestInfo *pRequest = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(paRequests);
    *paRequests = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // start by retrieving all address for this wallet
    ABC_CHECK_RET(ABC_TxGetAddresses(szUserName, szPassword, szWalletUUID, &aAddresses, &count, pError));

    // if there are any addresses
    if (count > 0)
    {
        // print out the addresses - debug
        //ABC_TxPrintAddresses(aAddresses, count);

        // walk through all the addresses looking for those with outstanding balances
        for (int i = 0; i < count; i++)
        {
            tABC_TxAddress *pAddr = aAddresses[i];
            ABC_CHECK_NULL(pAddr);
            tABC_TxDetails  *pDetails = pAddr->pDetails;

            // if this address has user details associated with it (i.e., was created by the user)
            if (pDetails)
            {
                tTxAddressStateInfo *pState = pAddr->pStateInfo;
                ABC_CHECK_NULL(pState);

                // if this is not a recyclable address (i.e., it was specifically used for a transaction)
                if (pState->bRecycleable == false)
                {
                    // if this address was used for a request of funds (i.e., not a send)
                    int64_t requestSatoshi = pDetails->amountSatoshi;
                    if (requestSatoshi >= 0)
                    {
                        // get the outstanding balanace on this request/address
                        int64_t owedSatoshi = 0;
                        ABC_CHECK_RET(ABC_TxGetAddressOwed(pAddr, &owedSatoshi, pError));

                        // if money is still owed
                        if (owedSatoshi > 0)
                        {
                            // create this request
                            ABC_ALLOC(pRequest, sizeof(tABC_RequestInfo));
                            ABC_STRDUP(pRequest->szID, pAddr->szID);
                            pRequest->timeCreation = pState->timeCreation;
                            pRequest->owedSatoshi = owedSatoshi;
                            pRequest->amountSatoshi = pDetails->amountSatoshi - owedSatoshi;
                            pRequest->pDetails = pDetails; // steal the info as is
                            pDetails = NULL;
                            pAddr->pDetails = NULL; // we are taking this for our own so later we don't want to free it

                            // increase the array size
                            if (countPending > 0)
                            {
                                countPending++;
                                ABC_REALLOC(aRequests, countPending * sizeof(tABC_RequestInfo *));
                            }
                            else
                            {
                                ABC_ALLOC(aRequests, sizeof(tABC_RequestInfo *));
                                countPending = 1;
                            }

                            // add it to the array
                            aRequests[countPending - 1] = pRequest;
                            pRequest = NULL;
                        }
                    }
                }
            }
        }
    }

    // assign final results
    *paRequests = aRequests;
    aRequests = NULL;
    *pCount = countPending;
    countPending = 0;

exit:
    ABC_TxFreeAddresses(aAddresses, count);
    ABC_TxFreeRequests(aRequests, countPending);
    ABC_TxFreeRequest(pRequest);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Given an address, this function returns the balance remaining on the address
 * It does this by checking the activity amounts against the initial request amount.
 * Negative indicates satoshi is still 'owed' on the address, 'positive' means excess was paid.
 *
 * the big assumption here is that address can be used for making payments after they have been used for
 * receiving payment but those should not be taken into account when determining what has been paid on the
 * address.
 *
 * @param pAddr          Address to check
 * @param pSatoshiOwed   Ptr into which owed amount is stored
 * @param pError         A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxGetAddressOwed(tABC_TxAddress *pAddr,
                             int64_t *pSatoshiOwed,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(pSatoshiOwed);
    *pSatoshiOwed = 0;
    ABC_CHECK_NULL(pAddr);
    tABC_TxDetails  *pDetails = pAddr->pDetails;
    ABC_CHECK_NULL(pDetails);
    tTxAddressStateInfo *pState = pAddr->pStateInfo;
    ABC_CHECK_NULL(pState);

    // start with the amount requested
    int64_t satoshiOwed = pDetails->amountSatoshi;

    // if any activities have occured on this address
    if ((pState->aActivities != NULL) && (pState->countActivities > 0))
    {
        for (int i = 0; i < pState->countActivities; i++)
        {
            // if this activity is money paid on the address
            // note: here is where negative activity is ignored
            // the big assumption here is that address can be used
            // for making payments after they have been used for
            // receiving payment but those should not be taken into
            // account when determining what has been paid on the
            // address
            if (pState->aActivities[i].amountSatoshi > 0)
            {
                // remove that from the request amount
                satoshiOwed -= pState->aActivities[i].amountSatoshi;
            }
        }
    }

    // assign final balance
    *pSatoshiOwed = satoshiOwed;

exit:

    return cc;
}


/**
 * Frees the given requets
 *
 * @param pRequest Ptr to request to free
 */
static
void ABC_TxFreeRequest(tABC_RequestInfo *pRequest)
{
    if (pRequest)
    {
        ABC_TxFreeDetails(pRequest->pDetails);

        ABC_CLEAR_FREE(pRequest, sizeof(tABC_RequestInfo));
    }
}


/**
 * Frees the given array of requets
 *
 * @param aRequests Array of requests
 * @param count     Number of requests
 */
void ABC_TxFreeRequests(tABC_RequestInfo **aRequests,
                        unsigned int count)
{
    if (aRequests && count > 0)
    {
        for (int i = 0; i < count; i++)
        {
            ABC_TxFreeRequest(aRequests[i]);
        }

        ABC_FREE(aRequests);
    }
}

/**
 * Gets the filename for a given transaction
 * format is: N-Base58(HMAC256(TxID,MK)).json
 *
 * @param pszFilename Output filename name. The caller must free this.
 */
static
tABC_CC ABC_TxCreateTxFilename(char **pszFilename, const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szTxID, bool bInternal, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szTxDir = NULL;
    tABC_U08Buf MK = ABC_BUF_NULL;
    tABC_U08Buf DataHMAC = ABC_BUF_NULL;
    char *szDataBase58 = NULL;

    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_NULL(szTxID);

    // Get the master key we will need to encode the filename
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    ABC_CHECK_RET(ABC_WalletGetTxDirName(&szTxDir, szWalletUUID, pError));

    // create an hmac-256 of the TxID
    tABC_U08Buf TxID = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(TxID, (unsigned char *)szTxID, strlen(szTxID));
    ABC_CHECK_RET(ABC_CryptoHMAC256(TxID, MK, &DataHMAC, pError));

    // create a base58 of the hmac-256 TxID
    ABC_CHECK_RET(ABC_CryptoBase58Encode(DataHMAC, &szDataBase58, pError));

    ABC_ALLOC(*pszFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszFilename, "%s/%s%s", szTxDir, szDataBase58, (bInternal ? TX_INTERNAL_SUFFIX : TX_EXTERNAL_SUFFIX));

exit:
    ABC_FREE_STR(szTxDir);
    ABC_BUF_FREE(DataHMAC);
    ABC_FREE_STR(szDataBase58);

    return cc;
}

/**
 * Loads a transaction from disk
 *
 * @param ppTx  Pointer to location to hold allocated transaction
 *              (it is the callers responsiblity to free this transaction)
 */
static
tABC_CC ABC_TxLoadTransaction(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              const char *szFilename,
                              tABC_Tx **ppTx,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf MK = ABC_BUF_NULL;
    json_t *pJSON_Root = NULL;
    tABC_Tx *pTx = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No filename provided");
    ABC_CHECK_NULL(ppTx);
    *ppTx = NULL;

    // Get the master key we will need to decode the transaction data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    // make sure the transaction exists
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

    // load the json object (load file, decrypt it, create json object
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, MK, &pJSON_Root, pError));

    // start decoding

    ABC_ALLOC(pTx, sizeof(tABC_Tx));

    // get the id
    json_t *jsonVal = json_object_get(pJSON_Root, JSON_TX_ID_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing id");
    ABC_STRDUP(pTx->szID, json_string_value(jsonVal));

    // get the state object
    ABC_CHECK_RET(ABC_TxDecodeTxState(pJSON_Root, &(pTx->pStateInfo), pError));

    // get the details object
    ABC_CHECK_RET(ABC_TxDecodeTxDetails(pJSON_Root, &(pTx->pDetails), pError));

    // assign final result
    *ppTx = pTx;
    pTx = NULL;

exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    ABC_TxFreeTx(pTx);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Decodes the transaction state data from a json transaction object
 *
 * @param ppInfo Pointer to store allocated state info
 *               (it is the callers responsiblity to free this)
 */
static
tABC_CC ABC_TxDecodeTxState(json_t *pJSON_Obj, tTxStateInfo **ppInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tTxStateInfo *pInfo = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppInfo);
    *ppInfo = NULL;

    // allocate the struct
    ABC_ALLOC(pInfo, sizeof(tTxStateInfo));

    // get the state object
    json_t *jsonState = json_object_get(pJSON_Obj, JSON_TX_STATE_FIELD);
    ABC_CHECK_ASSERT((jsonState && json_is_object(jsonState)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing state");

    // get the creation date
    json_t *jsonVal = json_object_get(jsonState, JSON_CREATION_DATE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing creation date");
    pInfo->timeCreation = json_integer_value(jsonVal);

    // get the internal boolean
    jsonVal = json_object_get(jsonState, JSON_TX_INTERNAL_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_boolean(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing internal boolean");
    pInfo->bInternal = json_is_true(jsonVal) ? true : false;

    // assign final result
    *ppInfo = pInfo;
    pInfo = NULL;

exit:
    ABC_CLEAR_FREE(pInfo, sizeof(tTxStateInfo));

    return cc;
}

/**
 * Decodes the transaction details data from a json transaction or address object
 *
 * @param ppInfo Pointer to store allocated meta info
 *               (it is the callers responsiblity to free this)
 */
static
tABC_CC ABC_TxDecodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails **ppDetails, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxDetails *pDetails = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppDetails);
    *ppDetails = NULL;

    // allocated the struct
    ABC_ALLOC(pDetails, sizeof(tABC_TxDetails));

    // get the details object
    json_t *jsonDetails = json_object_get(pJSON_Obj, JSON_DETAILS_FIELD);
    ABC_CHECK_ASSERT((jsonDetails && json_is_object(jsonDetails)), ABC_CC_JSONError, "Error parsing JSON details package - missing meta data (details)");

    // get the satoshi field
    json_t *jsonVal = json_object_get(jsonDetails, JSON_AMOUNT_SATOSHI_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing satoshi amount");
    pDetails->amountSatoshi = json_integer_value(jsonVal);

    // get the currency field
    jsonVal = json_object_get(jsonDetails, JSON_TX_AMOUNT_CURRENCY_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_real(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing currency amount");
    pDetails->amountCurrency = json_real_value(jsonVal);

    // get the name field
    jsonVal = json_object_get(jsonDetails, JSON_TX_NAME_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing name");
    ABC_STRDUP(pDetails->szName, json_string_value(jsonVal));

    // get the category field
    jsonVal = json_object_get(jsonDetails, JSON_TX_CATEGORY_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing category");
    ABC_STRDUP(pDetails->szCategory, json_string_value(jsonVal));

    // get the notes field
    jsonVal = json_object_get(jsonDetails, JSON_TX_NOTES_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing notes");
    ABC_STRDUP(pDetails->szNotes, json_string_value(jsonVal));

    // get the attributes field
    jsonVal = json_object_get(jsonDetails, JSON_TX_ATTRIBUTES_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON details package - missing attributes");
    pDetails->attributes = (unsigned int) json_integer_value(jsonVal);

    // assign final result
    *ppDetails = pDetails;
    pDetails = NULL;

exit:
    ABC_TxFreeDetails(pDetails);

    return cc;
}

/**
 * Free's a tABC_Tx struct and all its elements
 */
static
void ABC_TxFreeTx(tABC_Tx *pTx)
{
    if (pTx)
    {
        ABC_FREE_STR(pTx->szID);
        ABC_TxFreeDetails(pTx->pDetails);
        ABC_CLEAR_FREE(pTx->pStateInfo, sizeof(tTxStateInfo));

        ABC_CLEAR_FREE(pTx, sizeof(tABC_Tx));
    }
}

// creates the transaction directory if needed
static
tABC_CC ABC_TxCreateTxDir(const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szTxDir = NULL;

    // get the transaction directory
    ABC_CHECK_RET(ABC_WalletGetTxDirName(&szTxDir, szWalletUUID, pError));

    // if transaction dir doesn't exist, create it
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szTxDir, &bExists, pError));
    if (true != bExists)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szTxDir, pError));
    }

exit:
    ABC_FREE_STR(szTxDir);

    return cc;
}

/**
 * Saves a transaction to disk
 *
 * @param pTx  Pointer to transaction data
 */
static
tABC_CC ABC_TxSaveTransaction(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              const tABC_Tx *pTx,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf MK = ABC_BUF_NULL;
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(pTx);
    ABC_CHECK_NULL(pTx->szID);
    ABC_CHECK_ASSERT(strlen(pTx->szID) > 0, ABC_CC_Error, "No transaction ID provided");
    ABC_CHECK_NULL(pTx->pStateInfo);

    // Get the master key we will need to encode the transaction data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    // create the json for the transaction
    pJSON_Root = json_object();
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_Error, "Could not create transaction JSON object");

    // set the ID
    json_object_set_new(pJSON_Root, JSON_TX_ID_FIELD, json_string(pTx->szID));

    // set the state info
    ABC_CHECK_RET(ABC_TxEncodeTxState(pJSON_Root, pTx->pStateInfo, pError));

    // set the details
    ABC_CHECK_RET(ABC_TxEncodeTxDetails(pJSON_Root, pTx->pDetails, pError));

    // create the transaction directory if needed
    ABC_CHECK_RET(ABC_TxCreateTxDir(szWalletUUID, pError));

    // get the filename for this transaction
    ABC_CHECK_RET(ABC_TxCreateTxFilename(&szFilename, szUserName, szPassword, szWalletUUID, pTx->szID, pTx->pStateInfo->bInternal, pError));

    // save out the transaction object to a file encrypted with the master key
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    ABC_FREE_STR(szFilename);
    if (pJSON_Root) json_decref(pJSON_Root);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Encodes the transaction state data into the given json transaction object
 *
 * @param pJSON_Obj Pointer to the json object into which the state data is stored.
 * @param pInfo     Pointer to the state data to store in the json object.
 */
static
tABC_CC ABC_TxEncodeTxState(json_t *pJSON_Obj, tTxStateInfo *pInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    json_t *pJSON_State = NULL;
    int retVal = 0;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(pInfo);

    // create the state object
    pJSON_State = json_object();

    // add the creation date to the state object
    retVal = json_object_set_new(pJSON_State, JSON_CREATION_DATE_FIELD, json_integer(pInfo->timeCreation));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the internal boolean (internally created or created due to bitcoin event)
    retVal = json_object_set_new(pJSON_State, JSON_TX_INTERNAL_FIELD, json_boolean(pInfo->bInternal));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the state object to the master object
    retVal = json_object_set(pJSON_Obj, JSON_TX_STATE_FIELD, pJSON_State);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

exit:
    if (pJSON_State) json_decref(pJSON_State);

    return cc;
}

/**
 * Encodes the transaction details data into the given json transaction object
 *
 * @param pJSON_Obj Pointer to the json object into which the details are stored.
 * @param pDetails  Pointer to the details to store in the json object.
 */
static
tABC_CC ABC_TxEncodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails *pDetails, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    json_t *pJSON_Details = NULL;
    int retVal = 0;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(pDetails);

    // create the details object
    pJSON_Details = json_object();

    // add the satoshi field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_AMOUNT_SATOSHI_FIELD, json_integer(pDetails->amountSatoshi));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the currency field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_AMOUNT_CURRENCY_FIELD, json_real(pDetails->amountCurrency));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the name field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_NAME_FIELD, json_string(pDetails->szName));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the category field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_CATEGORY_FIELD, json_string(pDetails->szCategory));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the notes field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_NOTES_FIELD, json_string(pDetails->szNotes));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the attributes field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_ATTRIBUTES_FIELD, json_integer(pDetails->attributes));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the details object to the master object
    retVal = json_object_set(pJSON_Obj, JSON_DETAILS_FIELD, pJSON_Details);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

exit:
    if (pJSON_Details) json_decref(pJSON_Details);
    
    return cc;
}

/**
 * This function is used to support sorting an array of tTxInfo pointers via qsort.
 * qsort has the following documentation for the required function:
 *
 * Pointer to a function that compares two elements.
 * This function is called repeatedly by qsort to compare two elements. It shall follow the following prototype:
 *
 * int compar (const void* p1, const void* p2);
 *
 * Taking two pointers as arguments (both converted to const void*). The function defines the order of the elements by returning (in a stable and transitive manner):
 * return value	meaning
 * <0	The element pointed by p1 goes before the element pointed by p2
 * 0	The element pointed by p1 is equivalent to the element pointed by p2
 * >0	The element pointed by p1 goes after the element pointed by p2
 *
 */
static
int ABC_TxInfoPtrCompare (const void * a, const void * b)
{
    tABC_TxInfo **ppInfoA = (tABC_TxInfo **)a;
    tABC_TxInfo *pInfoA = (tABC_TxInfo *)*ppInfoA;
    tABC_TxInfo **ppInfoB = (tABC_TxInfo **)b;
    tABC_TxInfo *pInfoB = (tABC_TxInfo *)*ppInfoB;

    if (pInfoA->timeCreation < pInfoB->timeCreation) return -1;
    if (pInfoA->timeCreation == pInfoB->timeCreation) return 0;
    if (pInfoA->timeCreation > pInfoB->timeCreation) return 1;

    return 0;
}

/**
 * Sets the recycle status on an address as specified
 *
 * @param szUserName    UserName for the account associated with this request
 * @param szPassword    Password for the account associated with this request
 * @param szWalletUUID  UUID of the wallet associated with this request
 * @param szAddressID   ID of the address
 * @param ppAddress     Pointer to location to store allocated address info
 * @param pError        A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadAddress(const char *szUserName,
                          const char *szPassword,
                          const char *szWalletUUID,
                          const char *szAddressID,
                          tABC_TxAddress **ppAddress,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFile = NULL;
    char *szAddrDir = NULL;
    char *szFilename = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szAddressID);
    ABC_CHECK_ASSERT(strlen(szAddressID) > 0, ABC_CC_Error, "No address ID provided");
    ABC_CHECK_NULL(ppAddress);

    // get the filename for this address
    ABC_CHECK_RET(ABC_GetAddressFilename(szWalletUUID, szAddressID, &szFile, pError));

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // create the full filename
    ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH + 1);
    sprintf(szFilename, "%s/%s", szAddrDir, szFile);

    // load the request address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(szUserName, szPassword, szWalletUUID, szFilename, ppAddress, pError));

exit:
    ABC_FREE_STR(szFile);
    ABC_FREE_STR(szAddrDir);
    ABC_FREE_STR(szFilename);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Loads an address from disk given filename (complete path)
 *
 * @param ppAddress  Pointer to location to hold allocated address
 *                   (it is the callers responsiblity to free this address)
 */
static
tABC_CC ABC_TxLoadAddressFile(const char *szUserName,
                              const char *szPassword,
                              const char *szWalletUUID,
                              const char *szFilename,
                              tABC_TxAddress **ppAddress,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf MK = ABC_BUF_NULL;
    json_t *pJSON_Root = NULL;
    tABC_TxAddress *pAddress = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No filename provided");
    ABC_CHECK_NULL(ppAddress);
    *ppAddress = NULL;

    // Get the master key we will need to decode the transaction data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    // make sure the addresss exists
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoRequest, "Request address does not exist");

    // load the json object (load file, decrypt it, create json object
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, MK, &pJSON_Root, pError));

    // start decoding

    ABC_ALLOC(pAddress, sizeof(tABC_TxAddress));

    // get the seq and id
    json_t *jsonVal = json_object_get(pJSON_Root, JSON_ADDR_SEQ_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing seq");
    pAddress->seq = (uint32_t)json_integer_value(jsonVal);
    ABC_ALLOC(pAddress->szID, TX_MAX_ADDR_ID_LENGTH);
    sprintf(pAddress->szID, "%u", pAddress->seq);

    // get the public address field
    jsonVal = json_object_get(pJSON_Root, JSON_ADDR_ADDRESS_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing address");
    ABC_STRDUP(pAddress->szPubAddress, json_string_value(jsonVal));

    // get the state object
    ABC_CHECK_RET(ABC_TxDecodeAddressStateInfo(pJSON_Root, &(pAddress->pStateInfo), pError));

    // get the details object
    ABC_CHECK_RET(ABC_TxDecodeTxDetails(pJSON_Root, &(pAddress->pDetails), pError));

    // assign final result
    *ppAddress = pAddress;
    pAddress = NULL;

exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    ABC_TxFreeAddress(pAddress);
    
    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Decodes the address state info from a json address object
 *
 * @param ppState Pointer to store allocated state info
 *               (it is the callers responsiblity to free this)
 */
static
tABC_CC ABC_TxDecodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo **ppState, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tTxAddressStateInfo *pState = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppState);
    *ppState = NULL;

    // allocated the struct
    ABC_ALLOC(pState, sizeof(tTxAddressStateInfo));

    // get the state object
    json_t *jsonState = json_object_get(pJSON_Obj, JSON_ADDR_STATE_FIELD);
    ABC_CHECK_ASSERT((jsonState && json_is_object(jsonState)), ABC_CC_JSONError, "Error parsing JSON address package - missing state info");

    // get the creation date
    json_t *jsonVal = json_object_get(jsonState, JSON_CREATION_DATE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON transaction package - missing creation date");
    pState->timeCreation = json_integer_value(jsonVal);

    // get the internal boolean
    jsonVal = json_object_get(jsonState, JSON_ADDR_RECYCLEABLE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_boolean(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing recycleable boolean");
    pState->bRecycleable = json_is_true(jsonVal) ? true : false;

    // get the activity array (if it exists)
    json_t *jsonActivity = json_object_get(jsonState, JSON_ADDR_ACTIVITY_FIELD);
    if (jsonActivity)
    {
        ABC_CHECK_ASSERT(json_is_array(jsonActivity), ABC_CC_JSONError, "Error parsing JSON address package - missing activity array");

        // get the number of elements in the array
        pState->countActivities = (int) json_array_size(jsonActivity);

        if (pState->countActivities > 0)
        {
            ABC_ALLOC(pState->aActivities, sizeof(tTxAddressActivity) * pState->countActivities);

            for (int i = 0; i < pState->countActivities; i++)
            {
                json_t *pJSON_Elem = json_array_get(jsonActivity, i);
                ABC_CHECK_ASSERT((pJSON_Elem && json_is_object(pJSON_Elem)), ABC_CC_JSONError, "Error parsing JSON address package - missing activity array element");

                // get the tx id
                jsonVal = json_object_get(pJSON_Elem, JSON_TX_ID_FIELD);
                ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing activity txid");
                ABC_STRDUP(pState->aActivities[i].szTxID, json_string_value(jsonVal));

                // get the date field
                jsonVal = json_object_get(pJSON_Elem, JSON_ADDR_DATE_FIELD);
                ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing date");
                pState->aActivities[i].timeCreation = json_integer_value(jsonVal);

                // get the satoshi field
                jsonVal = json_object_get(pJSON_Elem, JSON_AMOUNT_SATOSHI_FIELD);
                ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_JSONError, "Error parsing JSON address package - missing satoshi amount");
                pState->aActivities[i].amountSatoshi = json_integer_value(jsonVal);
            }
        }
    }
    else
    {
        pState->countActivities = 0;
    }

    // assign final result
    *ppState = pState;
    pState = NULL;

exit:
    ABC_TxFreeAddressStateInfo(pState);
    
    return cc;
}

/**
 * Saves an address to disk
 *
 * @param pAddress  Pointer to address data
 */
static
tABC_CC ABC_TxSaveAddress(const char *szUserName,
                          const char *szPassword,
                          const char *szWalletUUID,
                          const tABC_TxAddress *pAddress,
                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf MK = ABC_BUF_NULL;
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(pAddress);
    ABC_CHECK_NULL(pAddress->szID);
    ABC_CHECK_ASSERT(strlen(pAddress->szID) > 0, ABC_CC_Error, "No address ID provided");
    ABC_CHECK_NULL(pAddress->pStateInfo);

    // Get the master key we will need to encode the address data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    // create the json for the transaction
    pJSON_Root = json_object();
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_Error, "Could not create address JSON object");

    // set the seq
    json_object_set_new(pJSON_Root, JSON_ADDR_SEQ_FIELD, json_integer(pAddress->seq));

    // set the address
    json_object_set_new(pJSON_Root, JSON_ADDR_ADDRESS_FIELD, json_string(pAddress->szPubAddress));

    // set the state info
    ABC_CHECK_RET(ABC_TxEncodeAddressStateInfo(pJSON_Root, pAddress->pStateInfo, pError));

    // set the details
    ABC_CHECK_RET(ABC_TxEncodeTxDetails(pJSON_Root, pAddress->pDetails, pError));

    // create the address directory if needed
    ABC_CHECK_RET(ABC_TxCreateAddressDir(szWalletUUID, pError));

    // create the filename for this transaction
    ABC_CHECK_RET(ABC_TxCreateAddressFilename(&szFilename, szUserName, szPassword, szWalletUUID, pAddress, pError));

    // save out the transaction object to a file encrypted with the master key
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFileObject(pJSON_Root, MK, ABC_CryptoType_AES256, szFilename, pError));

exit:
    ABC_FREE_STR(szFilename);
    if (pJSON_Root) json_decref(pJSON_Root);

    ABC_TxMutexUnlock(NULL);
    return cc;
}

/**
 * Encodes the address state data into the given json transaction object
 *
 * @param pJSON_Obj Pointer to the json object into which the state info is stored.
 * @param pInfo     Pointer to the state info to store in the json object.
 */
static
tABC_CC ABC_TxEncodeAddressStateInfo(json_t *pJSON_Obj, tTxAddressStateInfo *pInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    json_t *pJSON_State = NULL;
    json_t *pJSON_ActivityArray = NULL;
    json_t *pJSON_Activity = NULL;
    int retVal = 0;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(pInfo);

    // create the state object
    pJSON_State = json_object();

    // add the creation date to the state object
    retVal = json_object_set_new(pJSON_State, JSON_CREATION_DATE_FIELD, json_integer(pInfo->timeCreation));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the recycleable boolean
    retVal = json_object_set_new(pJSON_State, JSON_ADDR_RECYCLEABLE_FIELD, json_boolean(pInfo->bRecycleable));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // create the array object
    pJSON_ActivityArray = json_array();

    // if there are any activities
    if ((pInfo->countActivities > 0) && (pInfo->aActivities != NULL))
    {
        for (int i = 0; i < pInfo->countActivities; i++)
        {
            // create the array object
            pJSON_Activity = json_object();

            // add the ntxid to the activity object
            retVal = json_object_set_new(pJSON_Activity, JSON_TX_ID_FIELD, json_string(pInfo->aActivities[i].szTxID));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            // add the date to the activity object
            retVal = json_object_set_new(pJSON_Activity, JSON_ADDR_DATE_FIELD, json_integer(pInfo->aActivities[i].timeCreation));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            // add the amount satoshi to the activity object
            retVal = json_object_set_new(pJSON_Activity, JSON_AMOUNT_SATOSHI_FIELD, json_integer(pInfo->aActivities[i].amountSatoshi));
            ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

            // add this activity to the activity array
            json_array_append_new(pJSON_ActivityArray, pJSON_Activity);

            // the appent_new stole the reference so we are done with it
            pJSON_Activity = NULL;
        }
    }

    // add the activity array to the state object
    retVal = json_object_set(pJSON_State, JSON_ADDR_ACTIVITY_FIELD, pJSON_ActivityArray);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the state object to the master object
    retVal = json_object_set(pJSON_Obj, JSON_ADDR_STATE_FIELD, pJSON_State);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

exit:
    if (pJSON_State) json_decref(pJSON_State);
    if (pJSON_ActivityArray) json_decref(pJSON_ActivityArray);
    if (pJSON_Activity) json_decref(pJSON_Activity);

    return cc;
}

/**
 * Gets the filename for a given address
 * format is: N-Base58(HMAC256(pub_address,MK)).json
 *
 * @param pszFilename Output filename name. The caller must free this.
 */
static
tABC_CC ABC_TxCreateAddressFilename(char **pszFilename, const char *szUserName, const char *szPassword, const char *szWalletUUID, const tABC_TxAddress *pAddress, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szAddrDir = NULL;
    tABC_U08Buf MK = ABC_BUF_NULL;
    tABC_U08Buf DataHMAC = ABC_BUF_NULL;
    char *szDataBase58 = NULL;

    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_NULL(pAddress);

    // Get the master key we will need to encode the filename
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // create an hmac-256 of the public address
    tABC_U08Buf PubAddress = ABC_BUF_NULL;
    ABC_BUF_SET_PTR(PubAddress, (unsigned char *)pAddress->szPubAddress, strlen(pAddress->szPubAddress));
    ABC_CHECK_RET(ABC_CryptoHMAC256(PubAddress, MK, &DataHMAC, pError));

    // create a base58 of the hmac-256 public address
    ABC_CHECK_RET(ABC_CryptoBase58Encode(DataHMAC, &szDataBase58, pError));

    // create the filename
    ABC_ALLOC(*pszFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszFilename, "%s/%u-%s.json", szAddrDir, pAddress->seq, szDataBase58);

exit:
    ABC_FREE_STR(szAddrDir);
    ABC_BUF_FREE(DataHMAC);
    ABC_FREE_STR(szDataBase58);
    
    return cc;
}

// creates the address directory if needed
static
tABC_CC ABC_TxCreateAddressDir(const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szAddrDir = NULL;

    // get the address directory
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // if transaction dir doesn't exist, create it
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szAddrDir, &bExists, pError));
    if (true != bExists)
    {
        ABC_CHECK_RET(ABC_FileIOCreateDir(szAddrDir, pError));
    }

exit:
    ABC_FREE_STR(szAddrDir);

    return cc;
}

/**
 * Free's a ABC_TxFreeAddress struct and all its elements
 */
static
void ABC_TxFreeAddress(tABC_TxAddress *pAddress)
{
    if (pAddress)
    {
        ABC_FREE_STR(pAddress->szID);
        ABC_FREE_STR(pAddress->szPubAddress);
        ABC_TxFreeDetails(pAddress->pDetails);
        ABC_TxFreeAddressStateInfo(pAddress->pStateInfo);

        ABC_CLEAR_FREE(pAddress, sizeof(tABC_TxAddress));
    }
}

/**
 * Free's a tTxAddressStateInfo struct and all its elements
 */
static
void ABC_TxFreeAddressStateInfo(tTxAddressStateInfo *pInfo)
{
    if (pInfo)
    {
        if ((pInfo->aActivities != NULL) && (pInfo->countActivities > 0))
        {
            for (int i = 0; i < pInfo->countActivities; i++)
            {
                ABC_FREE_STR(pInfo->aActivities[i].szTxID);
            }
            ABC_CLEAR_FREE(pInfo->aActivities, sizeof(tTxAddressActivity) * pInfo->countActivities);
        }

        ABC_CLEAR_FREE(pInfo, sizeof(tTxAddressStateInfo));
    }
}

/**
 * Free's an array of  ABC_TxFreeAddress structs
 */
static
void ABC_TxFreeAddresses(tABC_TxAddress **aAddresses, unsigned int count)
{
    if ((aAddresses != NULL) && (count > 0))
    {
        for (int i = 0; i < count; i++)
        {
            ABC_TxFreeAddress(aAddresses[i]);
        }

        ABC_CLEAR_FREE(aAddresses, sizeof(tABC_TxAddress *) * count);
    }
}

/**
 * Gets the addresses associated with the given wallet.
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param paAddresses       Pointer to store array of addresses info pointers
 * @param pCount            Pointer to store number of addresses
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxGetAddresses(const char *szUserName,
                           const char *szPassword,
                           const char *szWalletUUID,
                           tABC_TxAddress ***paAddresses,
                           unsigned int *pCount,
                           tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szAddrDir = NULL;
    tABC_FileIOList *pFileList = NULL;
    char *szFilename = NULL;
    tABC_TxAddress **aAddresses = NULL;
    unsigned int count = 0;

    ABC_CHECK_RET(ABC_FileIOMutexLock(pError)); // we want this as an atomic files system function
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(paAddresses);
    *paAddresses = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // validate the creditials
    ABC_CHECK_RET(ABC_WalletCheckCredentials(szUserName, szPassword, szWalletUUID, pError));

    // get the directory name
    ABC_CHECK_RET(ABC_WalletGetAddressDirName(&szAddrDir, szWalletUUID, pError));

    // if there is a address directory
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szAddrDir, &bExists, pError));

    if (bExists == true)
    {
        ABC_ALLOC(szFilename, ABC_FILEIO_MAX_PATH_LENGTH + 1);

        // get all the files in the address directory
        ABC_FileIOCreateFileList(&pFileList, szAddrDir, NULL);
        for (int i = 0; i < pFileList->nCount; i++)
        {
            // if this file is a normal file
            if (pFileList->apFiles[i]->type == ABC_FileIOFileType_Regular)
            {
                // create the filename for this address
                sprintf(szFilename, "%s/%s", szAddrDir, pFileList->apFiles[i]->szName);

                // add this address to the array
                ABC_CHECK_RET(ABC_TxLoadAddressAndAppendToArray(szUserName, szPassword, szWalletUUID, szFilename, &aAddresses, &count, pError));
            }
        }
    }

    // if we have more than one, then let's sort them
    if (count > 1)
    {
        // sort the transactions by creation date using qsort
        qsort(aAddresses, count, sizeof(tABC_TxAddress *), ABC_TxAddrPtrCompare);
    }

    // store final results
    *paAddresses = aAddresses;
    aAddresses = NULL;
    *pCount = count;
    count = 0;

exit:
    ABC_FREE_STR(szAddrDir);
    ABC_FREE_STR(szFilename);
    ABC_FileIOFreeFileList(pFileList);
    ABC_TxFreeAddresses(aAddresses, count);
    
    ABC_FileIOMutexUnlock(NULL);
    return cc;
}

/**
 * This function is used to support sorting an array of tTxAddress pointers via qsort.
 * qsort has the following documentation for the required function:
 *
 * Pointer to a function that compares two elements.
 * This function is called repeatedly by qsort to compare two elements. It shall follow the following prototype:
 *
 * int compar (const void* p1, const void* p2);
 *
 * Taking two pointers as arguments (both converted to const void*). The function defines the order of the elements by returning (in a stable and transitive manner):
 * return value	meaning
 * <0	The element pointed by p1 goes before the element pointed by p2
 * 0	The element pointed by p1 is equivalent to the element pointed by p2
 * >0	The element pointed by p1 goes after the element pointed by p2
 *
 */
static
int ABC_TxAddrPtrCompare(const void * a, const void * b)
{
    tABC_TxAddress **ppInfoA = (tABC_TxAddress **)a;
    tABC_TxAddress *pInfoA = (tABC_TxAddress *)*ppInfoA;
    tABC_TxAddress **ppInfoB = (tABC_TxAddress **)b;
    tABC_TxAddress *pInfoB = (tABC_TxAddress *)*ppInfoB;

    if (pInfoA->seq < pInfoB->seq) return -1;
    if (pInfoA->seq == pInfoB->seq) return 0;
    if (pInfoA->seq > pInfoB->seq) return 1;

    return 0;
}

/**
 * Loads the given address and adds it to the end of the array
 *
 * @param szUserName        UserName for the account associated with the transactions
 * @param szPassword        Password for the account associated with the transactions
 * @param szWalletUUID      UUID of the wallet associated with the transactions
 * @param szFilename        Filename of address
 * @param paAddress         Pointer to array into which the address will be added
 * @param pCount            Pointer to store number of address (will be updated)
 * @param pError            A pointer to the location to store the error if there is one
 */
static
tABC_CC ABC_TxLoadAddressAndAppendToArray(const char *szUserName,
                                          const char *szPassword,
                                          const char *szWalletUUID,
                                          const char *szFilename,
                                          tABC_TxAddress ***paAddresses,
                                          unsigned int *pCount,
                                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_TxAddress *pAddress = NULL;
    tABC_TxAddress **aAddresses = NULL;
    unsigned int count = 0;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_ASSERT(strlen(szFilename) > 0, ABC_CC_Error, "No transaction filename provided");
    ABC_CHECK_NULL(paAddresses);
    ABC_CHECK_NULL(pCount);

    // hold on to current values
    count = *pCount;
    aAddresses = *paAddresses;

    // load the address
    ABC_CHECK_RET(ABC_TxLoadAddressFile(szUserName, szPassword, szWalletUUID, szFilename, &pAddress, pError));

    // create space for new entry
    if (aAddresses == NULL)
    {
        ABC_ALLOC(aAddresses, sizeof(tABC_TxAddress *));
        count = 1;
    }
    else
    {
        count++;
        ABC_REALLOC(aAddresses, sizeof(tABC_TxAddress *) * count);
    }

    // add it to the array
    aAddresses[count - 1] = pAddress;
    pAddress = NULL;

    // assign the values to the caller
    *paAddresses = aAddresses;
    *pCount = count;

exit:
    ABC_TxFreeAddress(pAddress);

    return cc;
}

#if 0
/**
 * For debug purposes, this prints all of the addresses in the given array
 */
static
void ABC_TxPrintAddresses(tABC_TxAddress **aAddresses, unsigned int count)
{
    if ((aAddresses != NULL) && (count > 0))
    {
        for (int i = 0; i < count; i++)
        {
            ABC_DebugLog("Address - seq: %lld, id: %s, pubAddress: %s\n", aAddresses[i]->seq, aAddresses[i]->szID, aAddresses[i]->szPubAddress);
            if (aAddresses[i]->pDetails)
            {
                ABC_DebugLog("\tDetails - satoshi: %lld, currency: %lf, name: %s, category: %s, notes: %s, attributes: %u\n",
                             aAddresses[i]->pDetails->amountSatoshi,
                             aAddresses[i]->pDetails->amountCurrency,
                             aAddresses[i]->pDetails->szName,
                             aAddresses[i]->pDetails->szCategory,
                             aAddresses[i]->pDetails->szNotes,
                             aAddresses[i]->pDetails->attributes);
            }
            else
            {
                ABC_DebugLog("\tNo Details");
            }
            if (aAddresses[i]->pStateInfo)
            {
                ABC_DebugLog("\tState Info - timeCreation: %lld, recycleable: %s\n",
                             aAddresses[i]->pStateInfo->timeCreation,
                             aAddresses[i]->pStateInfo->bRecycleable ? "yes" : "no");
                if (aAddresses[i]->pStateInfo->aActivities && aAddresses[i]->pStateInfo->countActivities)
                {
                    for (int nActivity = 0; nActivity < aAddresses[i]->pStateInfo->countActivities; nActivity++)
                    {
                        ABC_DebugLog("\t\tActivity - txID: %s, timeCreation: %lld, satoshi: %lld\n",
                                     aAddresses[i]->pStateInfo->aActivities[nActivity].szTxID,
                                     aAddresses[i]->pStateInfo->aActivities[nActivity].timeCreation,
                                     aAddresses[i]->pStateInfo->aActivities[nActivity].amountSatoshi);
                    }
                }
                else
                {
                    ABC_DebugLog("\t\tNo Activities");
                }
            }
            else
            {
                ABC_DebugLog("\tNo State Info");
            }
        }
        typedef struct sTxAddressActivity
        {
            char    *szTxID; // ntxid from bitcoin associated with this activity
            int64_t timeCreation;
            int64_t amountSatoshi;
        } tTxAddressActivity;

        typedef struct sTxAddressStateInfo
        {
            int64_t             timeCreation;
            bool                bRecycleable;
            unsigned int        countActivities;
            tTxAddressActivity  *aActivities;
        } tTxAddressStateInfo;

        typedef struct sABC_TxAddress
        {
            int64_t             seq; // sequence number
            char                *szID; // sequence number in string form
            char                *szPubAddress; // public address
            tABC_TxDetails      *pDetails;
            tTxAddressStateInfo *pStateInfo;
        } tABC_TxAddress;
    }
    else
    {
        ABC_DebugLog("No addresses");
    }
}
#endif

/**
 * Locks the mutex
 *
 * ABC_Tx uses the same mutex as ABC_Account/ABC_Wallet so that there will be no situation in
 * which one thread is in ABC_Tx locked on a mutex and calling a thread safe ABC_Account/ABC_Wallet call
 * that is locked from another thread calling a thread safe ABC_Tx call.
 * In other words, since they call each other, they need to share a recursive mutex.
 */
static
tABC_CC ABC_TxMutexLock(tABC_Error *pError)
{
    return ABC_MutexLock(pError);
}

/**
 * Unlocks the mutex
 */
static
tABC_CC ABC_TxMutexUnlock(tABC_Error *pError)
{
    return ABC_MutexUnlock(pError);
}

