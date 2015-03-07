/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/crypto/Encoding.hpp"
#include "../minilibs/catch/catch.hpp"

TEST_CASE("RFC 4648 base32 test vectors", "[crypto][base32]")
{
    struct TestCase
    {
        const char *data;
        const char *base32;
    };
    TestCase cases[] =
    {
        {"", ""},
        {"f", "MY======"},
        {"fo", "MZXQ===="},
        {"foo", "MZXW6==="},
        {"foob", "MZXW6YQ="},
        {"fooba", "MZXW6YTB"},
        {"foobar", "MZXW6YTBOI======"}
    };

    SECTION("encode")
    {
        for (auto &test: cases)
        {
            REQUIRE(test.base32 == abcd::base32Encode(std::string(test.data)));
        }
    }
    SECTION("decode")
    {
        for (auto &test: cases)
        {
            abcd::DataChunk result;
            REQUIRE(abcd::base32Decode(result, test.base32));
            REQUIRE(abcd::toString(result) == test.data);
        }
    }
}

TEST_CASE("Bad base32 strings", "[crypto][base32]")
{
    abcd::DataChunk result;

    SECTION("wrong length")
    {
        REQUIRE_FALSE(abcd::base32Decode(result, "12345"));
    }
    SECTION("too much padding")
    {
        REQUIRE_FALSE(abcd::base32Decode(result, "AAAAAAAA========"));
    }
    SECTION("illegal characters")
    {
        REQUIRE_FALSE(abcd::base32Decode(result, "A1======"));
        REQUIRE_FALSE(abcd::base32Decode(result, "Aa======"));
    }
}
