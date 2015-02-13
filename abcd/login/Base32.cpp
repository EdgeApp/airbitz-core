/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "OtpKey.hpp"
#include <algorithm>

namespace abcd {

std::string
base32Encode(DataSlice data)
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

bool
base32Decode(DataChunk &result, const std::string &in)
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

} // namespace abcd
