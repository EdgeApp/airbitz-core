/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_CRYPTO_OTPKEY_HPP
#define ABCD_CRYPTO_OTPKEY_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"

namespace abcd {

/**
 * Implements the TOTP algorithm defined by rfc6238.
 */
class OtpKey
{
public:
    OtpKey() {}
    OtpKey(DataSlice key): key_(key.begin(), key.end()) {}

    /**
     * Initializes the key with random data.
     */
    Status
    create(size_t keySize=10);

    /**
     * Initializes the key with a base32-encoded string.
     */
    Status
    decodeBase32(const std::string &key);

    /**
     * Produces a counter-based password.
     */
    std::string
    hotp(uint64_t counter, unsigned digits=6) const;

    /**
     * Produces a time-based password.
     */
    std::string
    totp(uint64_t timeStep=30, unsigned digits=6) const;

    /**
     * Encodes the key as a base32 string.
     */
    std::string
    encodeBase32() const;

    /**
     * Obtains access to the underlying binary key.
     */
    DataSlice
    key() const { return key_; }

private:
    DataChunk key_;
};

} // namespace abcd

#endif
