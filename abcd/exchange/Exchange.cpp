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
#include <sstream>

namespace abcd {

#define SATOSHI_PER_BITCOIN                     100000000

#define EXCHANGE_RATE_DIRECTORY "Exchanges/"

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

static tABC_CC ABC_ExchangeNeedsUpdate(int currencyNum, bool *bUpdateRequired, double *szRate, tABC_Error *pError);
static tABC_CC ABC_ExchangeGetFilename(char **pszFilename, int currencyNum, tABC_Error *pError);
static tABC_CC ABC_ExchangeExtractSource(tABC_ExchangeRateSources &sources, int currencyNum, char **szSource, tABC_Error *pError);

/**
 * Fetches the current rate and requests a new value if the current value is old.
 */
tABC_CC ABC_ExchangeCurrentRate(int currencyNum, double *pRate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    double rate;
    time_t lastUpdated;
    if (exchangeCacheGet(currencyNum, rate, lastUpdated))
    {
        *pRate = rate;
    }
    else
    {
        bool bUpdateRequired = true; // Ignored
        ABC_CHECK_RET(ABC_ExchangeNeedsUpdate(currencyNum, &bUpdateRequired, pRate, pError));
    }

exit:
    return cc;
}

tABC_CC ABC_ExchangeUpdate(tABC_ExchangeRateSources &sources, int currencyNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szSource = NULL;
    double rate = 0;
    bool bUpdateRequired = true;

    ABC_CHECK_RET(ABC_ExchangeNeedsUpdate(currencyNum, &bUpdateRequired, &rate, pError));
    if (bUpdateRequired)
    {
        ABC_CHECK_RET(ABC_ExchangeExtractSource(sources, currencyNum, &szSource, pError));
        if (szSource)
        {
            std::stringstream rateStr;
            AutoString szFilename;

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
            rateStr.precision(10);
            rateStr << rate << '\n';

            // Right changes to disk
            ABC_CHECK_RET(ABC_ExchangeGetFilename(&szFilename.get(), currencyNum, pError));
            ABC_CHECK_NEW(fileSave(rateStr.str(), szFilename.get()), pError);

            // Update the cache
            ABC_CHECK_NEW(exchangeCacheSet(currencyNum, rate), pError);
        }
    }
exit:
    return cc;
}

static
tABC_CC ABC_ExchangeNeedsUpdate(int currencyNum, bool *bUpdateRequired, double *pRate, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char *szFilename = NULL;
    time_t timeNow = time(NULL);
    bool bExists = false;

    double rate;
    time_t lastUpdated;
    if (exchangeCacheGet(currencyNum, rate, lastUpdated))
    {
        if ((timeNow - lastUpdated) < ABC_EXCHANGE_RATE_REFRESH_INTERVAL_SECONDS)
        {
            *bUpdateRequired = false;
        }
        *pRate = rate;
    }
    else
    {
        ABC_CHECK_RET(ABC_ExchangeGetFilename(&szFilename, currencyNum, pError));
        ABC_CHECK_RET(ABC_FileIOFileExists(szFilename, &bExists, pError));
        if (true == bExists)
        {
            DataChunk data;
            ABC_CHECK_NEW(fileLoad(data, szFilename), pError);
            // Set the exchange rate
            *pRate = strtod(toString(data).c_str(), NULL);
            // get the time the file was last changed
            time_t timeFileMod;
            ABC_CHECK_RET(ABC_FileIOFileModTime(szFilename, &timeFileMod, pError));

            // if it isn't too old then don't update
            if ((timeNow - timeFileMod) < ABC_EXCHANGE_RATE_REFRESH_INTERVAL_SECONDS)
            {
                *bUpdateRequired = false;
            }
        }
        else
        {
            *bUpdateRequired = true;
            *pRate = 0.0;
        }

        ABC_CHECK_NEW(exchangeCacheSet(currencyNum, *pRate), pError);
    }
exit:
    ABC_FREE_STR(szFilename);
    return cc;
}

static
tABC_CC ABC_ExchangeGetFilename(char **pszFilename, int currencyNum, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    std::string rateDir = getRootDir() + EXCHANGE_RATE_DIRECTORY;
    std::stringstream filename;

    ABC_CHECK_NULL(pszFilename);
    *pszFilename = NULL;

    ABC_CHECK_NEW(fileEnsureDir(rateDir), pError);

    filename << rateDir << currencyNum << ".txt";
    ABC_STRDUP(*pszFilename, filename.str().c_str());

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

    double rate;
    ABC_CHECK_OLD(ABC_ExchangeCurrentRate(static_cast<int>(currency), &rate, &error));

    result = in * (rate / SATOSHI_PER_BITCOIN);
    return Status();
}

Status
exchangeCurrencyToSatoshi(int64_t &result, double in, Currency currency)
{
    result = 0;

    double rate;
    ABC_CHECK_OLD(ABC_ExchangeCurrentRate(static_cast<int>(currency), &rate, &error));

    result = static_cast<int64_t>(in * (SATOSHI_PER_BITCOIN / rate));
    return Status();
}

} // namespace abcd
