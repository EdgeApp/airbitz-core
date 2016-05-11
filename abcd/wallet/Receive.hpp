/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_WALLET_RECEIVE_HPP
#define ABCD_WALLET_RECEIVE_HPP

#include "../util/Status.hpp"

namespace abcd {

struct TxInfo;
class Wallet;

/**
 * Updates the wallet when a new transaction comes in from the network.
 */
Status
onReceive(Wallet &wallet, const TxInfo &info,
          tABC_BitCoin_Event_Callback fCallback, void *pData);

} // namespace abcd

#endif
