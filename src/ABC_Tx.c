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

#define SATOSHI_PER_BITCOIN 100000000

#define CURRENCY_NUM_USD    840

static tABC_CC  ABC_TxSend(tABC_TxSendInfo *pInfo, char **pszUUID, tABC_Error *pError);
static tABC_CC  ABC_TxDupDetails(tABC_TxDetails **ppNewDetails, const tABC_TxDetails *pOldDetails, tABC_Error *pError);
static void     ABC_TxFreeDetails(tABC_TxDetails *pDetails);
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
     Ask libwallet for data required to send the amount
        input:
            amount
            list of source addresses
        output:
            fundable (do we have the funds),
            source addresses used,
                change address needed (any change?),
        fee
     If there is need for a ‘change address’, look for an address with the recycle bit set, if none found, ask libwallet to create a Bitcoin_Address (for change) using N+1 where N is the current number of addresses.
     Ask libwallet to send the amount to the address
        input: dest address, source address list (provided in step 1), amount and change Bitcoin_Address (if needed).
        output: TX ID.
     If there is a change address, create an Address Entry with the Bitcoin_Address and the send funds meta data (e.g., name, amount, etc).
     Create a transaction in the transaction table with meta data using the TX ID from step 3.
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
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param szURI     URI to parse
 * @param ppInfo    Pointer to location to store allocated info struct.
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxParseBitcoinURI(const char *szURI,
                            tABC_BitcoinURIInfo **ppInfo,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_BitcoinURIInfo *pInfo = NULL;

    ABC_CHECK_NULL(szURI);
    ABC_CHECK_ASSERT(strlen(szURI) > 0, ABC_CC_Error, "No URI provided");
    ABC_CHECK_NULL(ppInfo);
    *ppInfo = NULL;

    // allocated initial struct
    ABC_ALLOC(pInfo, sizeof(tABC_BitcoinURIInfo));

    // TODO: parse the elements and store them in the pInfo struct
    // Note: if a given member (e.g., label) doesn't exist, a blank string should still be allocated

    // assign created info struct
    *ppInfo = pInfo;
    pInfo = NULL; // do this so we don't free below what we just gave the caller

exit:
    ABC_FreeURIInfo(pInfo);

    return cc;
}

/**
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param pInfo Pointer to allocated info struct.
 */
void ABC_TxFreeURIInfo(tABC_BitcoinURIInfo *pInfo)
{
    if (pInfo != NULL)
    {
        ABC_FREE_STR(pInfo->szLabel);

        ABC_FREE_STR(pInfo->szAddress);

        ABC_FREE_STR(pInfo->szMessage);

        ABC_CLEAR_FREE(pInfo, sizeof(tABC_BitcoinURIInfo));
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
     creates an Address and returns ID (recycle bit set)
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

