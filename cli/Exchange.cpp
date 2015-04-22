/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Command.hpp"
#include "../abcd/exchange/ExchangeSource.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::context, ExchangeFetch, "exchange-fetch")
{
    for (const auto &source: exchangeSources)
    {
        ExchangeRates rates;
        ABC_CHECK(exchangeSourceFetch(rates, source));

        std::cout << source << ":" << std::endl;
        for (auto &i: rates)
        {
            std::string code, name;
            ABC_CHECK(currencyCode(code, i.first));
            ABC_CHECK(currencyName(name, i.first));
            std::cout << code << ": " << i.second << "\t# " << name << std::endl;
        }
        std::cout << std::endl;
    }

    return Status();
}
