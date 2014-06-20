/**
 * @file
 * AirBitz Exchange functions
 *
 * @author Tim Horton
 * @version 1.0
 */

#ifndef ABC_Exchanges_h
#define ABC_Exchanges_h

#include "ABC.h"
#include "ABC_Util.h"

#define ABC_BITSTAMP "Bitstamp"
#define ABC_COINBASE "Coinbase"

#ifdef __cplusplus
extern "C" {
#endif

    typedef enum eABC_Exchange
    {
        ABC_BitStamp = 0
    } tABC_Exchange;

    typedef struct sABC_ExchangeInfo
    {
        tABC_Exchange exchange;
        int           currencyNum;
        char          *szUserName;
        char          *szPassword;
    } tABC_ExchangeInfo;

    tABC_CC ABC_ExchangeInitialize(
                tABC_BitCoin_Event_Callback  fAsyncBitCoinEventCallback,
                void                         *pData,
                tABC_Error                   *pError);

    tABC_CC ABC_ExchangeCurrentRate(const char *szUserName, const char *szPassword,
                                    int currencyNum, double *pRate, tABC_Error *pError);

    void ABC_ExchangeTerminate();

#ifdef __cplusplus
}
#endif

#endif
