/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Utility functions that should probably go into libbitcoin one day.
 */

#ifndef ABCD_BITCOIN_UTILITY_HPP
#define ABCD_BITCOIN_UTILITY_HPP

#include "../util/Status.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

/**
 * Calculates the non-malleable id for a transaction.
 */
bc::hash_digest
makeNtxid(bc::transaction_type tx);

/**
 * Returns true if a transaction opts in to RBF semantics.
 */
bool
isReplaceByFee(const bc::transaction_type &tx);

/**
 * Bundles the provided data into a script push operation.
 */
bc::operation
makePushOperation(bc::data_slice data);

/**
 * Decodes a blob of raw data into a transaction.
 */
Status
decodeTx(bc::transaction_type &result, bc::data_slice rawTx);

} // namespace abcd

#endif
