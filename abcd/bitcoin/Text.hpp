/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Helpers for dealing with Bitcoin-related text formats.
 */

#ifndef ABCD_BITCOIN_TEXT_HPP
#define ABCD_BITCOIN_TEXT_HPP

#include "../util/Status.hpp"

namespace abcd {

/**
 * All the fields that can be found in a URI, bitcoin address, or private key.
 */
struct ParsedUri
{
    // Top-level actions:
    std::string address;
    std::string wif;
    std::string paymentProto;
    std::string bitidUri;
    std::string otpKey;

    // URI parameters:
    uint64_t amountSatoshi = 0;
    std::string label;
    std::string message;
    std::string category; // Airbitz extension
    std::string ret; // Airbitz extension
};

/**
 * Decodes a URI, bitcoin address, or private key.
 */
Status
parseUri(ParsedUri &result, const std::string &text);

/**
 * Generate a random hbits private key.
 */
Status
hbitsCreate(std::string &result, std::string &addressOut);

} // namespace abcd

#endif
