/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_SPEND_OUTPUTS_HPP
#define ABCD_SPEND_OUTPUTS_HPP

#include "../util/Status.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

bc::script_type
outputScriptForPubkey(const bc::short_hash &hash);

/**
 * Creates an output script for sending money to an address.
 */
Status
outputScriptForAddress(bc::script_type &result, const std::string &address);

/**
 * Returns true if an amount is small enough to be considered dust.
 */
bool
outputIsDust(uint64_t amount);

/**
 * Add a change output, sort the outputs, and check for dust.
 */
Status
outputsFinalize(bc::transaction_output_list &outputs,
                uint64_t change, const std::string &changeAddress);

uint64_t
outputsTotal(const bc::transaction_output_list &outputs);

} // namespace abcd

#endif
