/*
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 */
/**
 * @file
 * AirBitz exchange-rate conversion functions.
 */

#ifndef ABC_Exchanges_h
#define ABC_Exchanges_h

#include "../src/ABC.h"
#include "util/Sync.hpp"
#include <stddef.h>

namespace abcd {

#define CURRENCY_NUM_AUD                 36
#define CURRENCY_NUM_CAD                124
#define CURRENCY_NUM_CNY                156
#define CURRENCY_NUM_CUP                192
#define CURRENCY_NUM_HKD                344
#define CURRENCY_NUM_MXN                484
#define CURRENCY_NUM_NZD                554
#define CURRENCY_NUM_PHP                608
#define CURRENCY_NUM_GBP                826
#define CURRENCY_NUM_USD                840
#define CURRENCY_NUM_EUR                978

#define ABC_BITSTAMP "Bitstamp"
#define ABC_COINBASE "Coinbase"
#define ABC_BNC      "BraveNewCoin"

/**
 * AirBitz Exchange Info Structure
 */
typedef struct sABC_ExchangeInfo
{
    /** Access to the account settings */
    tABC_SyncKeys         *pKeys;
    /** The currency to request or update **/
    int                   currencyNum;
    /** Callback fired after a update **/
    tABC_Request_Callback fRequestCallback;
    /** Data to return with the callback **/
    void                  *pData;
} tABC_ExchangeInfo;

/**
 * AirBitz default exchange info
 */
typedef struct sABC_ExchangeDefaults {
    int currencyNum;
    const char *szDefaultExchange;
} tABC_ExchangeDefaults;

// Default Exchange array
extern const tABC_ExchangeDefaults EXCHANGE_DEFAULTS[];
extern const size_t EXCHANGE_DEFAULTS_SIZE;

tABC_CC ABC_ExchangeInitialize(tABC_Error                   *pError);

tABC_CC ABC_ExchangeCurrentRate(tABC_SyncKeys *pKeys,
                                int currencyNum, double *pRate, tABC_Error *pError);

tABC_CC ABC_ExchangeUpdate(tABC_ExchangeInfo *pInfo, tABC_Error *pError);

void *ABC_ExchangeUpdateThreaded(void *pData);

void ABC_ExchangeTerminate();

tABC_CC ABC_ExchangeAlloc(tABC_SyncKeys *pKeys,
                          int currencyNum,
                          tABC_Request_Callback fRequestCallback, void *pData,
                          tABC_ExchangeInfo **ppInfo, tABC_Error *pError);
void ABC_ExchangeFreeInfo(tABC_ExchangeInfo *pInfo);

} // namespace abcd

#endif
