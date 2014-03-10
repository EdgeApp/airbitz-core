/**
 * @file
 * AirBitz Tx function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_Tx_h
#define ABC_Tx_h

#include "ABC.h"
#include "ABC_Util.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * AirBitz Core Send Tx Structure
     *
     * This structure contains the detailed information associated
     * with initiating a send.
     *
     */
    typedef struct sABC_TxSendInfo
    {
        /** data pointer given by caller at initial create call time */
        void                    *pData;

        char                    *szUserName;
        char                    *szPassword;
        char                    *szWalletUUID;
        char                    *szDestAddress;
        tABC_TxDetails          *pDetails;

        tABC_Request_Callback   fRequestCallback;

        /** information the error if there was a failure */
        tABC_Error  errorInfo;
    } tABC_TxSendInfo;


    tABC_CC ABC_TxSendInfoAlloc(tABC_TxSendInfo **ppTxSendInfo,
                                const char *szUserName,
                                const char *szPassword,
                                const char *szWalletUUID,
                                const char *szDestAddress,
                                const tABC_TxDetails *pDetails,
                                tABC_Request_Callback fRequestCallback,
                                void *pData,
                                tABC_Error *pError);


    void ABC_TxSendInfoFree(tABC_TxSendInfo *pTxSendInfo);

    void * ABC_TxSendThreaded(void *pData);

    tABC_CC ABC_TxParseBitcoinURI(const char *szURI,
                                tABC_BitcoinURIInfo **ppInfo,
                                tABC_Error *pError);

    void ABC_TxFreeURIInfo(tABC_BitcoinURIInfo *pInfo);

    double ABC_TxSatoshiToBitcoin(int64_t satoshi);

    int64_t ABC_TxBitcoinToSatoshi(double bitcoin);

    tABC_CC ABC_TxSatoshiToCurrency(int64_t satoshi,
                                    double *pCurrency,
                                    int currencyNum,
                                    tABC_Error *pError);

    tABC_CC ABC_TxCurrencyToSatoshi(double currency,
                                    int currencyNum,
                                    int64_t *pSatoshi,
                                    tABC_Error *pError);

    tABC_CC ABC_TxCreateReceiveRequest(const char *szUserName,
                                       const char *szPassword,
                                       const char *szWalletUUID,
                                       tABC_TxDetails *pDetails,
                                       char **pszRequestID,
                                       tABC_Error *pError);

    tABC_CC ABC_TxModifyReceiveRequest(const char *szUserName,
                                       const char *szPassword,
                                       const char *szWalletUUID,
                                       const char *szRequestID,
                                       tABC_TxDetails *pDetails,
                                       tABC_Error *pError);

    tABC_CC ABC_TxFinalizeReceiveRequest(const char *szUserName,
                                         const char *szPassword,
                                         const char *szWalletUUID,
                                         const char *szRequestID,
                                         tABC_Error *pError);

    tABC_CC ABC_TxCancelReceiveRequest(const char *szUserName,
                                       const char *szPassword,
                                       const char *szWalletUUID,
                                       const char *szRequestID,
                                       tABC_Error *pError);

    tABC_CC ABC_TxGenerateRequestQRCode(const char *szUserName,
                                        const char *szPassword,
                                        const char *szWalletUUID,
                                        const char *szRequestID,
                                        unsigned char **paData,
                                        unsigned int *pWidth,
                                        tABC_Error *pError);

    tABC_CC ABC_TxGetTransactions(const char *szUserName,
                                  const char *szPassword,
                                  const char *szWalletUUID,
                                  tABC_TxInfo ***paTransactions,
                                  unsigned int *pCount,
                                  tABC_Error *pError);

    void ABC_TxFreeTransactions(tABC_TxInfo **aTransactions,
                                unsigned int count);

    tABC_CC ABC_TxSetTransactionDetails(const char *szUserName,
                                        const char *szPassword,
                                        const char *szWalletUUID,
                                        const char *szID,
                                        tABC_TxDetails *pDetails,
                                        tABC_Error *pError);

    tABC_CC ABC_TxGetPendingRequests(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     tABC_RequestInfo ***paRequests,
                                     unsigned int *pCount,
                                     tABC_Error *pError);

    void ABC_TxFreeRequests(tABC_RequestInfo **aRequests,
                            unsigned int count);
    
#ifdef __cplusplus
}
#endif

#endif
