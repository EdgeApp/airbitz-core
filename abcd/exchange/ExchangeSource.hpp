/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_EXCHANGE_EXCHANGE_SOURCE_HPP
#define ABCD_EXCHANGE_EXCHANGE_SOURCE_HPP

#include "Currency.hpp"
#include <list>
#include <map>

namespace abcd {

typedef std::list<std::string> ExchangeSources;
typedef std::map<Currency, double> ExchangeRates;

/**
 * All the exchange-rate sources implemented in the core.
 */
extern const ExchangeSources exchangeSources;

/**
 * Fetches the exchange rates from a particular source.
 */
Status
exchangeSourceFetch(ExchangeRates &result, const std::string &source);

} // namespace abcd

#endif
