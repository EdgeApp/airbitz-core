/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Helpers for dealing with Airbitz transaction metadata.
 */

#ifndef SRC_TX_DETAILS_HPP
#define SRC_TX_DETAILS_HPP

#include "../abcd/util/Status.hpp"

namespace abcd {

/**
 * Free a TX details struct
 */
void
ABC_TxDetailsFree(tABC_TxDetails *pDetails);

} // namespace abcd

#endif
