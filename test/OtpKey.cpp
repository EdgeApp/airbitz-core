/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/login/OtpKey.hpp"
#include "../abcd/login/Base32.hpp"
#include "../minilibs/catch/catch.hpp"

TEST_CASE("RFC 4226 test vectors", "[login][otp][otpkey]" )
{
    abcd::DataChunk secret
    {
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30,
        0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x30
    };
    const char *cases[] =
    {
        "755224",
        "287082",
        "359152",
        "969429",
        "338314",
        "254676",
        "287922",
        "162583",
        "399871",
        "520489"
    };

    abcd::OtpKey key;
    REQUIRE(key.decodeBase32(abcd::base32Encode(secret)));

    int i = 0;
    for (auto test: cases)
    {
        REQUIRE(key.hotp(i) == cases[i]);
        ++i;
    }
}

TEST_CASE("Leading zeros in OTP output", "[login][otp][otpkey]" )
{
    abcd::OtpKey key;
    REQUIRE(key.decodeBase32("AAAAAAAA"));
    REQUIRE(key.hotp(2) == "073348");
    REQUIRE(key.hotp(9) == "003773");
}
