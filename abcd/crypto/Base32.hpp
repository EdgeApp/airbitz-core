/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_CRYPTO_BASE32_HPP
#define ABCD_CRYPTO_BASE32_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"

namespace abcd {

/**
 * Encodes data into a base-32 string according to rfc4648.
 */
std::string
base32Encode(DataSlice data);

/**
 * Decodes a base-32 string as defined by rfc4648.
 */
bool
base32Decode(DataChunk &result, const std::string &in);

} // namespace abcd

#endif
