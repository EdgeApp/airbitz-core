/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_CRYPTO_SCRYPT_HPP
#define ABCD_CRYPTO_SCRYPT_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"

namespace abcd {

constexpr size_t scryptDefaultSize = 32;

/**
 * Parameters for the scrypt algorithm.
 */
struct ScryptSnrp
{
    DataChunk salt;
    uint64_t n;
    uint32_t r;
    uint32_t p;

    /**
     * Initializes the parameters with a random salt and
     * benchmarked difficulty settings.
     */
    Status
    create();

    /**
     * Initialize the parameters based on a time elapsed (in microseconds)
     * from a 16-1-1 hash. Used to determine final hashing parameters
     */
    void
    createSnrpFromTime(unsigned long totalTime);

    /**
     * The scrypt hash function.
     */
    Status
    hash(DataChunk &result, DataSlice data, unsigned long *time=nullptr,
         size_t size=scryptDefaultSize) const;
};

/**
 * Returns the fixed SNRP value used for the username.
 */
const ScryptSnrp &
usernameSnrp();

} // namespace abcd

#endif
