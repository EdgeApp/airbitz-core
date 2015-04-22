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

#include "Exchange.hpp"
#include "ExchangeCache.hpp"
#include "ExchangeServers.hpp"
#include "../util/FileIO.hpp"
#include <memory>
#include <mutex>

namespace abcd {

#define SATOSHI_PER_BITCOIN                     100000000

static std::mutex exchangeMutex;
static std::unique_ptr<ExchangeCache> exchangeCache;

#define ABC_BITSTAMP "Bitstamp"
#define ABC_COINBASE "Coinbase"
#define ABC_BNC      "BraveNewCoin"

const tABC_ExchangeDefaults EXCHANGE_DEFAULTS[] =
{
    {CURRENCY_NUM_AUD, ABC_BNC},
    {CURRENCY_NUM_CAD, ABC_BNC},
    {CURRENCY_NUM_CNY, ABC_BNC},
    {CURRENCY_NUM_CUP, ABC_COINBASE},
    {CURRENCY_NUM_HKD, ABC_BNC},
    {CURRENCY_NUM_MXN, ABC_BNC},
    {CURRENCY_NUM_NZD, ABC_BNC},
    {CURRENCY_NUM_PHP, ABC_COINBASE},
    {CURRENCY_NUM_GBP, ABC_BNC},
    {CURRENCY_NUM_USD, ABC_BITSTAMP},
    {CURRENCY_NUM_EUR, ABC_BNC},
};

const size_t EXCHANGE_DEFAULTS_SIZE = sizeof(EXCHANGE_DEFAULTS)
                                    / sizeof(tABC_ExchangeDefaults);

static tABC_CC ABC_ExchangeExtractSource(tABC_ExchangeRateSources &sources, int currencyNum, char **szSource, tABC_Error *pError);

/**
 * Loads the exchange cache from disk if it hasn't been done yet.
 */
static Status
exchangeCacheLoad()
{
    if (!exchangeCache)
    {
        exchangeCache.reset(new ExchangeCache(getRootDir()));
        exchangeCache->load(); // Nothing bad happens if this fails
    }
    return Status();
}

tABC_CC ABC_ExchangeUpdate(tABC_ExchangeRateSources &sources, int currencyNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    auto currency = static_cast<Currency>(currencyNum);
    time_t now = time(nullptr);

    std::lock_guard<std::mutex> lock(exchangeMutex);
    ABC_CHECK_NEW(exchangeCacheLoad(), pError);

    // Only update if the current entry is missing or old:
    if (!exchangeCache->fresh(currency, now))
    {
        double rate = 0;
        AutoString szSource;
        ABC_CHECK_RET(ABC_ExchangeExtractSource(sources, currencyNum, &szSource.get(), pError));

        if (szSource)
        {
            if (strcmp(ABC_BITSTAMP, szSource) == 0)
            {
                ABC_CHECK_RET(ABC_ExchangeBitStampRate(currencyNum, rate, pError));
            }
            else if (strcmp(ABC_COINBASE, szSource) == 0)
            {
                ABC_CHECK_RET(ABC_ExchangeCoinBaseRates(currencyNum, rate, pError));
            }
            else if (strcmp(ABC_BNC, szSource) == 0)
            {
                ABC_CHECK_RET(ABC_ExchangeBncRates(currencyNum, rate, pError));
            }

            ABC_CHECK_NEW(exchangeCache->update(currency, rate, now), pError);
            ABC_CHECK_NEW(exchangeCache->save(), pError);
        }
    }

exit:
    return cc;
}

static
tABC_CC ABC_ExchangeExtractSource(tABC_ExchangeRateSources &sources, int currencyNum,
                                  char **szSource, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    *szSource = NULL;
    for (unsigned i = 0; i < sources.numSources; i++)
    {
        if (sources.aSources[i]->currencyNum == currencyNum)
        {
            ABC_STRDUP(*szSource, sources.aSources[i]->szSource);
            break;
        }
    }
    if (!(*szSource))
    {
        // If the settings are not populated, defaults
        switch (currencyNum)
        {
            case CURRENCY_NUM_USD:
                ABC_STRDUP(*szSource, ABC_BITSTAMP);
                break;
            case CURRENCY_NUM_CAD:
            case CURRENCY_NUM_CNY:
            case CURRENCY_NUM_EUR:
            case CURRENCY_NUM_GBP:
            case CURRENCY_NUM_MXN:
            case CURRENCY_NUM_AUD:
            case CURRENCY_NUM_HKD:
            case CURRENCY_NUM_NZD:
                ABC_STRDUP(*szSource, ABC_BNC);
                break;
            case CURRENCY_NUM_CUP:
            case CURRENCY_NUM_PHP:
                ABC_STRDUP(*szSource, ABC_COINBASE);
                break;
            default:
                ABC_STRDUP(*szSource, ABC_BITSTAMP);
                break;
        }
    }

exit:
    return cc;
}

Status
exchangeSatoshiToCurrency(double &result, int64_t in, Currency currency)
{
    result = 0.0;

    std::lock_guard<std::mutex> lock(exchangeMutex);
    ABC_CHECK(exchangeCacheLoad());

    double rate;
    ABC_CHECK(exchangeCache->rate(rate, currency));

    result = in * (rate / SATOSHI_PER_BITCOIN);
    return Status();
}

Status
exchangeCurrencyToSatoshi(int64_t &result, double in, Currency currency)
{
    result = 0;

    std::lock_guard<std::mutex> lock(exchangeMutex);
    ABC_CHECK(exchangeCacheLoad());

    double rate;
    ABC_CHECK(exchangeCache->rate(rate, currency));

    result = static_cast<int64_t>(in * (SATOSHI_PER_BITCOIN / rate));
    return Status();
}

} // namespace abcd
