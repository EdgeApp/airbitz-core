/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "OtpKey.hpp"
#include "../util/Crypto.hpp"
#include "../util/U08Buf.hpp"
#include <openssl/hmac.h>
#include <time.h>
#include <algorithm>
#include <sstream>

namespace abcd {

/**
 * Encodes data into a base-32 string according to rfc4648.
 */
static std::string
encodeBase32(DataSlice data)
{
    const char base32Sym[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string out;
    auto chunks = (data.size() + 4) / 5; // Rounding up
    out.reserve(8 * chunks);

    auto i = data.begin();
    uint16_t buffer = 0; // Bits waiting to be written out, MSB first
    int bits = 0; // Number of bits currently in the buffer
    while (i != data.end() || 0 < bits)
    {
        // Reload the buffer if we need more bits:
        if (i != data.end() && bits < 5)
        {
            buffer |= *i++ << (8 - bits);
            bits += 8;
        }

        // Write out 5 most-significant bits in the buffer:
        out += base32Sym[buffer >> 11];
        buffer <<= 5;
        bits -= 5;
    }

    // Pad the final string to a multiple of 8 characters long:
    out.append(-out.size() % 8, '=');
    return out;
}

/**
 * Decodes a base-32 string as defined by rfc4648.
 */
static bool
decodeBase32(DataChunk &result, const std::string &in)
{
    // The string must be a multiple of 8 characters long:
    if (in.size() % 8)
        return false;

    DataChunk out;
    out.reserve(5 * (in.size() / 8));

    auto i = in.begin();
    uint16_t buffer = 0; // Bits waiting to be written out, MSB first
    int bits = 0; // Number of bits currently in the buffer
    while (i != in.end())
    {
        // Read one character from the string:
        int value = 0;
        if ('A' <= *i && *i <= 'Z')
            value = *i++ - 'A';
        else if ('2' <= *i && *i <= '7')
            value = 26 + *i++ - '2';
        else
            break;

        // Append the bits to the buffer:
        buffer |= value << (11 - bits);
        bits += 5;

        // Write out some bits if the buffer has a byte's worth:
        if (8 <= bits)
        {
            out.push_back(buffer >> 8);
            buffer <<= 8;
            bits -= 8;
        }
    }

    // Any extra characters must be '=':
    if (!std::all_of(i, in.end(), [](char c){ return '=' == c; }))
        return false;

    // There cannot be extra padding:
    if (8 <= in.end() - i)
        return false;

    // Any extra bits must be 0 (but rfc4648 decoders can be liberal here):
//    if (buffer != 0)
//        return false;

    result = std::move(out);
    return true;
}

Status
OtpKey::create(size_t keySize)
{
    AutoU08Buf rawKey;
    ABC_CHECK_OLD(ABC_CryptoCreateRandomData(keySize, &rawKey, &error));
    key_ = DataChunk(rawKey.p, rawKey.end);
    return Status();
}

Status
OtpKey::decodeBase32(const std::string &key)
{
    if (!abcd::decodeBase32(key_, key))
        return ABC_ERROR(ABC_CC_ParseError, "Key is not valid base32");
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
    return abcd::encodeBase32(key_);
}

} // namespace abcd
