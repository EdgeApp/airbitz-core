/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_SPEND_AIRBITZ_FEE_HPP
#define ABCD_SPEND_AIRBITZ_FEE_HPP

#include "../util/Status.hpp"

namespace abcd {

struct AirbitzFeeInfo;
class Wallet;

/**
 * Calculates the fee owed for a spend.
 */
int64_t
airbitzFeeOutgoing(const AirbitzFeeInfo &info, int64_t spent);

/**
 * Calculates the fee owed for a receive.
 */
int64_t
airbitzFeeIncoming(const AirbitzFeeInfo &info, int64_t received);

/**
 * Sends an Airbitz fee if one is owed, and enough time has passed.
 */
Status
airbitzFeeAutoSend(Wallet &wallet);

} // namespace abcd

#endif
