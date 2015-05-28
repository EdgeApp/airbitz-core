/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_BROADCAST_HPP
#define ABCD_BITCOIN_BROADCAST_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"

namespace abcd {

/**
 * Sends a transaction out to the Bitcoin network.
 */
Status
broadcastTx(DataSlice rawTx);

} // namespace abcd

#endif
