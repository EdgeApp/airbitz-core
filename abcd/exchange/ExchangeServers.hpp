/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Exchange-rate data providers.
 */

#ifndef ABCD_EXCHANGE_EXCHANGE_SERVERS_H
#define ABCD_EXCHANGE_EXCHANGE_SERVERS_H

#include "../../src/ABC.h"

namespace abcd {

tABC_CC ABC_ExchangeBitStampRate(int currencyNum, double &rate, tABC_Error *pError);
tABC_CC ABC_ExchangeCoinBaseRates(int currencyNum, double &rate, tABC_Error *pError);
tABC_CC ABC_ExchangeBncRates(int currencyNum, double &rate, tABC_Error *pError);

} // namespace abcd

#endif
