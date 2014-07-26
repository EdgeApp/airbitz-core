/**
 * @file
 * AirBitz Exchange functions
 *
 * See LICENSE for copy, modification, and use permissions 
 *
 * @author See AUTHORS
 * @version 1.0
 */

#ifndef ABC_Exchanges_h
#define ABC_Exchanges_h

#include "ABC.h"
#include "ABC_Util.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * AirBitz Exchange Info Structure
     */
    typedef struct sABC_ExchangeInfo
    {
        /** The currency to request or update **/
        int                   currencyNum;
        /** Username used to access account settings **/
        char                  *szUserName;
        /** Password used to access account settings **/
        char                  *szPassword;
        /** Callback fired after a update **/
        tABC_Request_Callback fRequestCallback;
        /** Data to return with the callback **/
        void                  *pData;
    } tABC_ExchangeInfo;

    tABC_CC ABC_ExchangeInitialize(
                tABC_BitCoin_Event_Callback  fAsyncBitCoinEventCallback,
                void                         *pData,
                tABC_Error                   *pError);

    tABC_CC ABC_ExchangeCurrentRate(const char *szUserName, const char *szPassword,
                                    int currencyNum, double *pRate, tABC_Error *pError);

    tABC_CC ABC_ExchangeUpdate(tABC_ExchangeInfo *pInfo, tABC_Error *pError);

    void *ABC_ExchangeUpdateThreaded(void *pData);

    void ABC_ExchangeTerminate();

    tABC_CC ABC_ExchangeAlloc(const char *szUserName, const char *szPassword,
                              int currencyNum,
                              tABC_Request_Callback fRequestCallback, void *pData,
                              tABC_ExchangeInfo **ppInfo, tABC_Error *pError);
    void ABC_ExchangeFreeInfo(tABC_ExchangeInfo *pInfo);

#ifdef __cplusplus
}
#endif

#endif
