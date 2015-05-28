/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_SPEND_OUTPUTS_HPP
#define ABCD_SPEND_OUTPUTS_HPP

#include "../util/Status.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

typedef struct sABC_TxSendInfo tABC_SendInfo;

/**
 * The minimum number of satoshis an output should contain.
 */
constexpr unsigned outputDust = 5460;

bc::script_type
outputScriptForPubkey(const bc::short_hash &hash);

/**
 * Creates an output script for sending money to an address.
 */
Status
outputScriptForAddress(bc::script_type &result, const std::string &address);

/**
 * Creates a set of outputs corresponding to a sABC_TxSendInfo structure.
 * Updates the info structure with the Airbitz fees, if any.
 */
Status
outputsForSendInfo(bc::transaction_output_list &result, sABC_TxSendInfo *pInfo);

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
