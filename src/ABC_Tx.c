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
#include <qrencode.h>
#include "ABC_Tx.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"
#include "ABC_Crypto.h"
#include "ABC_Account.h"
#include "ABC_Mutex.h"
#include "ABC_Wallet.h"

#define SATOSHI_PER_BITCOIN 100000000

#define CURRENCY_NUM_USD    840

#define JSON_TX_ID_FIELD                "ntxid"
#define JSON_TX_STATE_FIELD             "state"
#define JSON_TX_DETAILS_FIELD           "meta"
#define JSON_TX_CREATION_DATE_FIELD     "creationDate"
#define JSON_TX_AMOUNT_SATOSHI_FIELD    "amountSatoshi"
#define JSON_TX_AMOUNT_CURRENCY_FIELD   "amountCurrency"
#define JSON_TX_NAME_FIELD              "name"
#define JSON_TX_CATEGORY_FIELD          "category"
#define JSON_TX_NOTES_FIELD             "notes"
#define JSON_TX_ATTRIBUTES_FIELD        "attributes"

typedef struct sTxStateInfo
{
    int64_t timeCreation;
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
    int64_t             seq; // sequence number
    char                *szID; // sequence number in string form
    char                *szPubAddress; // public address
    tABC_TxDetails      *pDetails;
    tTxAddressStateInfo *pStateInfo;
} tABC_TxAddress;

static tABC_CC  ABC_TxSend(tABC_TxSendInfo *pInfo, char **pszUUID, tABC_Error *pError);
static tABC_CC  ABC_TxDupDetails(tABC_TxDetails **ppNewDetails, const tABC_TxDetails *pOldDetails, tABC_Error *pError);
static void     ABC_TxFreeDetails(tABC_TxDetails *pDetails);
static tABC_CC  ABC_TxGetTxFilename(char **pszFilename, const char *szWalletUUID, const char *szTxID, tABC_Error *pError);
static tABC_CC  ABC_TxLoadTransaction(const char *szUserName, const char *szPassword, const char *szWalletUUID, const char *szTxID, tABC_Tx **ppTx, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeTxState(json_t *pJSON_Obj, tTxStateInfo **ppInfo, tABC_Error *pError);
static tABC_CC  ABC_TxDecodeTxDetails(json_t *pJSON_Obj, tABC_TxDetails **ppDetails, tABC_Error *pError);
static void     ABC_TxFreeTx(tABC_Tx *pTx);
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

    pTxSendInfo->szUserName = strdup(szUserName);
    pTxSendInfo->szPassword = strdup(szPassword);
    pTxSendInfo->szWalletUUID = strdup(szWalletUUID);
    pTxSendInfo->szDestAddress = strdup(szDestAddress);

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
    *pszTxID = strdup("ID");

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
static
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
        pNewDetails->szName = strdup(pOldDetails->szName);
    }
    if (pOldDetails->szCategory != NULL)
    {
        pNewDetails->szCategory = strdup(pOldDetails->szCategory);
    }
    if (pOldDetails->szNotes != NULL)
    {
        pNewDetails->szNotes = strdup(pOldDetails->szNotes);
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
static
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

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(pDetails);
    ABC_CHECK_NULL(pszRequestID);
    *pszRequestID = NULL;

    // for now just create a place holder id
    *pszRequestID = strdup("ID");

    // TODO: write the real function -
    /*
    Core creates an Address Entry and returns ID (recycle bit set):
        1) Look for an address with the recycle bit set, if none found, ask libwallet to create a Bitcoin_Address using N+1 where N is the current number of addresses and create an Address Entry.
        2) Fill in the information (e.g., name, amount, etc) with the information provided for the Address Entry with recycle bit set.
        3) Return the Address Entry ID.
     */


exit:

    return cc;
}

/**
 * Modifies a previously created receive request.
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

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");
    ABC_CHECK_NULL(pDetails);

    // TODO: write the real function -
    /*
     modifies the meta data for the Address of the given request
     */

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

    // TODO: write the real function -
    /*
     clears recycle bit
     */

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

    // TODO: write the real function -
    /*
     Address as ‘unassociated/reusable’
     Note: this or recycle bit set means it can be reused but the mark of ‘unassociated/reusable’ helps determine actions of funds come in on the address. (i.e., we can remind the user that they cancelled this request)
     */

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

    QRcode *qr = NULL;
    unsigned char *aData = NULL;
    unsigned int length = 0;

    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szRequestID);
    ABC_CHECK_ASSERT(strlen(szRequestID) > 0, ABC_CC_Error, "No request ID provided");

    // TODO: write the real function -
    // for now just generate a temporary qr code
    char *szURITemp = "bitcoin:1NS17iag9jJgTHD1VXjvLCEnZuQ3rJED9L?amount=50&label=Luke-Jr&message=Donation%20for%20project%20xyz";
    qr = QRcode_encodeString(szURITemp, 0, QR_ECLEVEL_L, QR_MODE_8, 1);
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
    QRcode_free(qr);
    ABC_CLEAR_FREE(aData, length);
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

    // TODO: get the tranactions for this wallet

exit:
    return cc;
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
            tABC_TxInfo *pInfo = aTransactions[i];

            ABC_TxFreeDetails(pInfo->pDetails);

            ABC_CLEAR_FREE(pInfo, sizeof(tABC_TxInfo));
        }

        ABC_FREE(aTransactions);
    }
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
tABC_CC ABC_TxSetTransactionDetails(const char *szUserName,
                                    const char *szPassword,
                                    const char *szWalletUUID,
                                    const char *szID,
                                    tABC_TxDetails *pDetails,
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
    ABC_CHECK_NULL(szID);
    ABC_CHECK_ASSERT(strlen(szID) > 0, ABC_CC_Error, "No transaction ID provided");

    // TODO: set the details for the transaction

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
tABC_CC ABC_TxGetPendingRequests(const char *szUserName,
                                 const char *szPassword,
                                 const char *szWalletUUID,
                                 tABC_RequestInfo ***paRequests,
                                 unsigned int *pCount,
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
    ABC_CHECK_NULL(paRequests);
    *paRequests = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // TODO: get the pending requests for this wallet

exit:
    return cc;
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
            tABC_RequestInfo *pInfo = aRequests[i];

            ABC_TxFreeDetails(pInfo->pDetails);

            ABC_CLEAR_FREE(pInfo, sizeof(tABC_RequestInfo));
        }

        ABC_FREE(aRequests);
    }
}

/**
 * Gets the filename for a given transaction
 *
 * @param pszDir the output directory name. The caller must free this.
 */
static
tABC_CC ABC_TxGetTxFilename(char **pszFilename, const char *szWalletUUID, const char *szTxID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char *szTxDir = NULL;

    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_NULL(szTxID);

    ABC_CHECK_RET(ABC_WalletGetTxDirName(&szTxDir, szWalletUUID, pError));

    ABC_ALLOC(*pszFilename, ABC_FILEIO_MAX_PATH_LENGTH);
    sprintf(*pszFilename, "%s/%s.json", szTxDir, szTxID);

exit:
    ABC_FREE_STR(szTxDir);

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
                              const char *szTxID,
                              tABC_Tx **ppTx,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf MK = ABC_BUF_NULL;
    char *szFilename = NULL;
    json_t *pJSON_Root = NULL;
    tABC_Tx *pTx = NULL;

    ABC_CHECK_RET(ABC_TxMutexLock(pError));
    ABC_CHECK_NULL(szUserName);
    ABC_CHECK_ASSERT(strlen(szUserName) > 0, ABC_CC_Error, "No username provided");
    ABC_CHECK_NULL(szPassword);
    ABC_CHECK_ASSERT(strlen(szPassword) > 0, ABC_CC_Error, "No password provided");
    ABC_CHECK_NULL(szWalletUUID);
    ABC_CHECK_ASSERT(strlen(szWalletUUID) > 0, ABC_CC_Error, "No wallet UUID provided");
    ABC_CHECK_NULL(szTxID);
    ABC_CHECK_ASSERT(strlen(szTxID) > 0, ABC_CC_Error, "No transaction ID provided");
    ABC_CHECK_NULL(ppTx);
    *ppTx = NULL;

    // Get the master key we will need to decode the transaction data
    // (note that this will also make sure the account and wallet exist)
    ABC_CHECK_RET(ABC_WalletGetMK(szUserName, szPassword, szWalletUUID, &MK, pError));

    // get the filename for this transaction
    ABC_CHECK_RET(ABC_TxGetTxFilename(&szFilename, szWalletUUID, szTxID, pError));

    // make sure the transaction exists
    bool bExists = false;
    ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
    ABC_CHECK_ASSERT(bExists == true, ABC_CC_NoTransaction, "Transaction does not exist");

    // load the json object (load file, decrypt it, create json object
    ABC_CHECK_RET(ABC_CryptoDecryptJSONFileObject(szFilename, MK, &pJSON_Root, pError));

    // start decoding

    // get the id
    json_t *jsonVal = json_object_get(pJSON_Root, JSON_TX_ID_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON transaction package - missing id");
    pTx->szID = strdup(json_string_value(jsonVal));

    // get the state object
    ABC_CHECK_RET(ABC_TxDecodeTxState(pJSON_Root, &(pTx->pStateInfo), pError));

    // get the details object
    ABC_CHECK_RET(ABC_TxDecodeTxDetails(pJSON_Root, &(pTx->pDetails), pError));

    // assign final result
    *ppTx = pTx;
    pTx = NULL;

exit:
    ABC_FREE_STR(szFilename);
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

    // get the state object
    json_t *jsonState = json_object_get(pJSON_Obj, JSON_TX_STATE_FIELD);
    ABC_CHECK_ASSERT((jsonState && json_is_object(jsonState)), ABC_CC_DecryptError, "Error parsing JSON transaction package - missing state");

    // get the creation date
    json_t *jsonVal = json_object_get(jsonState, JSON_TX_CREATION_DATE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON transaction package - missing creation date");
    int64_t date = json_integer_value(jsonVal);

    // allocate the struct
    ABC_ALLOC(pInfo, sizeof(tTxStateInfo));
    pInfo->timeCreation = date;

    // assign final result
    *ppInfo = pInfo;
    pInfo = NULL;

exit:
    ABC_CLEAR_FREE(pInfo, sizeof(tTxStateInfo));

    return cc;
}

/**
 * Decodes the transaction details data from a json transaction object
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
    ABC_ALLOC(pDetails, sizeof(pDetails));

    // get the details object
    json_t *jsonDetails = json_object_get(pJSON_Obj, JSON_TX_DETAILS_FIELD);
    ABC_CHECK_ASSERT((jsonDetails && json_is_object(jsonDetails)), ABC_CC_DecryptError, "Error parsing JSON transaction package - missing meta data (details)");

    // get the satoshi field
    json_t *jsonVal = json_object_get(jsonDetails, JSON_TX_AMOUNT_SATOSHI_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON transaction package - missing satoshi amount");
    pDetails->amountSatoshi = json_integer_value(jsonVal);

    // get the currency field
    jsonVal = json_object_get(jsonDetails, JSON_TX_AMOUNT_CURRENCY_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_real(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON transaction package - missing currency amount");
    pDetails->amountCurrency = json_real_value(jsonVal);

    // get the name field
    jsonVal = json_object_get(jsonDetails, JSON_TX_NAME_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON transaction package - missing name");
    pDetails->szName = strdup(json_string_value(jsonVal));

    // get the category field
    jsonVal = json_object_get(jsonDetails, JSON_TX_CATEGORY_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON transaction package - missing category");
    pDetails->szCategory = strdup(json_string_value(jsonVal));

    // get the notes field
    jsonVal = json_object_get(jsonDetails, JSON_TX_NOTES_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON transaction package - missing notes");
    pDetails->szNotes = strdup(json_string_value(jsonVal));

    // get the attributes field
    jsonVal = json_object_get(jsonDetails, JSON_TX_ATTRIBUTES_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_integer(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON transaction package - missing attributes");
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

/* TODO: these support functions will be needed
 save transaction
 load address
 save address
 load transactions
 save transactions
 */

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

