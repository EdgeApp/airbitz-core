/**
 * @file
 * AirBitz Wallet function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_Wallet_h
#define ABC_Wallet_h

#include "ABC.h"

#ifdef __cplusplus
extern "C" {
#endif
    
    /**
     * AirBitz Core Create Wallet Structure
     *
     * This structure contains the detailed information associated
     * with creating a new wallet.
     *
     */
    typedef struct sABC_WalletCreateInfo
    {
        /** data pointer given by caller at initial create call time */
        void        *pData;
        
        const char *szUserName;
        const char *szPassword;
        const char *szWalletName;
        const char *szCurrency;
        tABC_Request_Callback fRequestCallback;
        
        /** information the error if there was a failure */
        tABC_Error  errorInfo;
    } tABC_WalletCreateInfo;
    
    
    tABC_CC ABC_WalletCreateInfoAlloc(tABC_WalletCreateInfo **ppWalletCreateInfo,
                                       const char *szUserName,
                                       const char *szPassword,
                                       const char *szWalletName,
                                       const char *szCurrency,
                                       tABC_Request_Callback fRequestCallback,
                                       void *pData,
                                       tABC_Error *pError);
    
    tABC_CC ABC_WalletCreateInfoFree(tABC_WalletCreateInfo *pWalletCreateInfo,
                                      tABC_Error *pError);
    
    void *ABC_WalletCreateThreaded(void *pData);
    
    
#ifdef __cplusplus
}
#endif

#endif
