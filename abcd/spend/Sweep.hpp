/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_SPEND_SPEND_HPP
#define ABCD_SPEND_SPEND_HPP

#include "../util/Status.hpp"

namespace abcd {

class Wallet;

/**
 * Sweeps the funds from an address into the wallet.
 * Requires that the address has been fully synced into the cache.
 */
void
sweepOnComplete(Wallet &wallet,
                const std::string &address, const std::string &wif,
                tABC_BitCoin_Event_Callback fCallback, void *pData);

} // namespace abcd

#endif
