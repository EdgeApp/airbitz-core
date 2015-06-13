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
#include "../util/FileIO.hpp"
#include <memory>
#include <mutex>

namespace abcd {

#define SATOSHI_PER_BITCOIN                     100000000

static std::mutex exchangeMutex;
static std::unique_ptr<ExchangeCache> exchangeCache;

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

Status
exchangeUpdate(Currencies currencies, const ExchangeSources &sources)
{
    std::lock_guard<std::mutex> lock(exchangeMutex);
    ABC_CHECK(exchangeCacheLoad());

    time_t now = time(nullptr);
    if (exchangeCache->fresh(currencies, now))
        return Status();

    ExchangeRates allRates;
    for (auto source: sources)
    {
        // Stop if the todo list is empty:
        if (currencies.empty())
            break;

        // Grab the rates from the server:
        ExchangeRates rates;
        if (!exchangeSourceFetch(rates, source))
            continue; // Just skip the failed ones

        // Remove any new rates from the todo list:
        for (auto rate: rates)
        {
            auto i = currencies.find(rate.first);
            if (currencies.end() != i)
                currencies.erase(i);
        }

        // Add any new rates to the allRates list:
        rates.insert(allRates.begin(), allRates.end());
        allRates = std::move(rates);
    }

    // Add the rates to the cache:
    for (auto rate: allRates)
        ABC_CHECK(exchangeCache->update(rate.first, rate.second, now));
    ABC_CHECK(exchangeCache->save());

    return Status();
}

Status
exchangeSatoshiToCurrency(double &result, int64_t in, Currency currency)
{
    result = 0.0;

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

    ABC_CHECK(exchangeCacheLoad());

    double rate;
    ABC_CHECK(exchangeCache->rate(rate, currency));

    result = static_cast<int64_t>(in * (SATOSHI_PER_BITCOIN / rate));
    return Status();
}

} // namespace abcd
