/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Currency.hpp"
#include <map>

namespace abcd {

// Currency list formatters:
#define CURRENCY_NUMBER_ROW(code, number, name) {{#code}, Currency::code},
#define CURRENCY_CODE_ROW(code, number, name)   {Currency::code, #code},
#define CURRENCY_NAME_ROW(code, number, name)   {Currency::code, name},

Status
currencyNumber(Currency &result, const std::string &code)
{
    static const std::map<std::string, Currency> map
    {
        ABC_CURRENCY_LIST(CURRENCY_NUMBER_ROW)
    };

    auto i = map.find(code);
    if (map.end() == i)
        return ABC_ERROR(ABC_CC_ParseError, "Cannot find currency code " + code);

    result = i->second;
    return Status();
}

Status
currencyCode(std::string &result, Currency number)
{
    static const std::map<Currency, const char *> map
    {
        ABC_CURRENCY_LIST(CURRENCY_CODE_ROW)
    };

    auto i = map.find(number);
    if (map.end() == i)
        return ABC_ERROR(ABC_CC_ParseError, "Cannot find currency number");

    result = i->second;
    return Status();
}

Status
currencyName(std::string &result, Currency number)
{
    static const std::map<Currency, const char *> map
    {
        ABC_CURRENCY_LIST(CURRENCY_NAME_ROW)
    };

    auto i = map.find(number);
    if (map.end() == i)
        return ABC_ERROR(ABC_CC_ParseError, "Cannot find currency number");

    result = i->second;
    return Status();
}

} // namespace abcd
