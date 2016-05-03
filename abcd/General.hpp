/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Airbitz general, non-account-specific server-supplied data.
 *
 * The data handled in this file is basically just a local cache of various
 * settings that Airbitz would like to adjust from time-to-time without
 * upgrading the entire app.
 */

#ifndef ABCD_GENERAL_HPP
#define ABCD_GENERAL_HPP

#include "bitcoin/Typedefs.hpp"
#include <map>
#include <vector>

namespace abcd {

/**
 * Maps from transaction sizes to corresponding fees.
 */
typedef std::map<size_t, uint64_t> BitcoinFeeInfo;

/**
 * Airbitz transaction fee information.
 */
struct AirbitzFeeInfo
{
    AddressSet addresses;

    // Fee amounts for incoming funds:
    double incomingRate;
    int64_t incomingMax;
    int64_t incomingMin;

    // Fee amounts for outgoing funds:
    double outgoingRate;
    int64_t outgoingMax;
    int64_t outgoingMin;
    int64_t noFeeMinSatoshi;

    // When to actually send fees:
    int64_t sendMin;
    time_t sendPeriod;
    std::string sendCategory;
    std::string sendPayee;
};

/**
 * Downloads general info from the server if the local file is out of date.
 */
Status
generalUpdate();

/**
 * Obtains the Bitcoin mining fee information.
 * The returned table always has at least one entry.
 */
BitcoinFeeInfo
generalBitcoinFeeInfo();

/**
 * Obtains the Airbitz fee information.
 * The fees will be zero if something goes wrong.
 */
AirbitzFeeInfo
generalAirbitzFeeInfo();

/**
 * Calculates the Airbitz fee for a particular transaction amount.
 */
uint64_t
generalAirbitzFee(const AirbitzFeeInfo &info, uint64_t spend, bool transfer);

/**
 * Obtains a list of libbitcoin servers for the current network
 * (either testnet or mainnet).
 * Returns a fallback server if something goes wrong.
 */
std::vector<std::string>
generalBitcoinServers();

/**
 * Obtains a list of sync servers.
 * Returns a fallback server if something goes wrong.
 */
std::vector<std::string>
generalSyncServers();

} // namespace abcd

#endif
