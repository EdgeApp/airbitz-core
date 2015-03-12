/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "OtpKey.hpp"
#include "Encoding.hpp"
#include "Random.hpp"
#include <openssl/hmac.h>
#include <time.h>
#include <sstream>

namespace abcd {

Status
OtpKey::create(size_t keySize)
{
    ABC_CHECK(randomData(key_, keySize));
    return Status();
}

Status
OtpKey::decodeBase32(const std::string &key)
{
    ABC_CHECK(base32Decode(key_, key));
    return Status();
}

std::string
OtpKey::hotp(uint64_t counter, unsigned digits) const
{
    // Do HMAC_SHA1(key_, counter):
    DataArray<20> hmac;
    DataArray<8> cb =
    {{
        static_cast<uint8_t>(counter >> 56),
        static_cast<uint8_t>(counter >> 48),
        static_cast<uint8_t>(counter >> 40),
        static_cast<uint8_t>(counter >> 32),
        static_cast<uint8_t>(counter >> 24),
        static_cast<uint8_t>(counter >> 16),
        static_cast<uint8_t>(counter >> 8),
        static_cast<uint8_t>(counter)
    }};
    HMAC(EVP_sha1(), key_.data(), key_.size(), cb.data(), cb.size(),
        hmac.data(), nullptr);

    // Calculate the truncated output:
    unsigned offset = hmac[19] & 0xf;
    uint32_t p = (hmac[offset] << 24) | (hmac[offset + 1] << 16) |
        (hmac[offset + 2] << 8) | hmac[offset + 3];
    p &= 0x7fffffff;

    // Format as a fixed-width decimal number:
    std::stringstream ss;
    ss.width(digits);
    ss.fill('0');
    ss << p;
    auto s = ss.str();
    s.erase(0, s.size() - digits);
    return s;
}

std::string
OtpKey::totp(uint64_t timeStep, unsigned digits) const
{
    return hotp(time(nullptr) / timeStep, digits);
}

std::string
OtpKey::encodeBase32() const
{
    return base32Encode(key_);
}

} // namespace abcd
