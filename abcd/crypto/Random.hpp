/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_CRYPTO_RANDOM_HPP
#define ABCD_CRYPTO_RANDOM_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"

namespace abcd {

tABC_CC ABC_CryptoSetRandomSeed(const tABC_U08Buf Seed,
                                tABC_Error        *pError);

tABC_CC ABC_CryptoCreateRandomData(unsigned int  length,
                                   tABC_U08Buf   *pData,
                                   tABC_Error    *pError);

tABC_CC ABC_CryptoGenUUIDString(char       **pszUUID,
                                tABC_Error *pError);

} // namespace abcd

#endif
