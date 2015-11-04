/*
 * Copyright (c) 2014, AirBitz, Inc.
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

#include "util/Status.hpp"
#include <map>
#include <set>
#include <vector>

namespace abcd {

typedef std::set<std::string> AddressSet;

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
    int64_t minSatoshi;
    int64_t maxSatoshi;
    double rate;
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
