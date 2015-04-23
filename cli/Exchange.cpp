/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Command.hpp"
#include "../abcd/exchange/Exchange.hpp"
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

COMMAND(InitLevel::account, ExchangeUpdate, "exchange-update")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... get-exchange-rate <user> <pass> <currency>");

    Currency currency;
    ABC_CHECK(currencyNumber(currency, argv[2]));
    ABC_CHECK_OLD(ABC_RequestExchangeRateUpdate(argv[0], argv[1], static_cast<int>(currency), &error));

    double rate;
    ABC_CHECK(exchangeSatoshiToCurrency(rate, 100000000, currency));
    std::cout << "result: " << rate << std::endl;

    return Status();
}

#define CURRENCY_SET_ROW(code, number, name) Currency::code,

/**
 * Verifies that all the currencies have sources.
 */
COMMAND(InitLevel::context, ExchangeValidate, "exchange-validate")
{
    Currencies currencies
    {
        ABC_CURRENCY_LIST(CURRENCY_SET_ROW)
    };

    // Eliminate any currencies the exchange sources provide:
    for (const auto &source: exchangeSources)
    {
        ExchangeRates rates;
        ABC_CHECK(exchangeSourceFetch(rates, source));

        for (auto &rate: rates)
        {
            auto i = currencies.find(rate.first);
            if (currencies.end() != i)
                currencies.erase(i);
        }
    }

    // Print an message if there is anything left:
    if (currencies.size())
    {
        std::cout << "The following currencies have no sources:" << std::endl;
        for (auto &currency: currencies)
        {
            std::string code;
            ABC_CHECK(currencyCode(code, currency));
            std::cout << code << std::endl;
        }
    }

    return Status();
}
