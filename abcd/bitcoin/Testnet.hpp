/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Routines for dealing with testnet/mainnet differences.
 */

#ifndef ABCD_BITCOIN_TESTNET_HPP
#define ABCD_BITCOIN_TESTNET_HPP

#include <stdint.h>

namespace abcd {

/**
 * Returns true if libbitcoin has been compiled with testnet support.
 */
bool isTestnet();

/**
 * Returns the version byte for a pubkey address.
 * Depends on whether or not testnet is turned on.
 */
uint8_t pubkeyVersion();

/**
 * Returns the version byte for a p2sh address.
 * Depends on whether or not testnet is turned on.
 */
uint8_t scriptVersion();

} // namespace abcd

#endif
