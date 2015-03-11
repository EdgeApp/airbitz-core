/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/crypto/Encoding.hpp"
#include "../minilibs/catch/catch.hpp"

TEST_CASE("RFC 4648 base16 test vectors", "[crypto][base16]")
{
    struct TestCase
    {
        const char *data;
        const char *text;
    };
    TestCase cases[] =
    {
        {"", ""},
        {"f", "66"},
        {"fo", "666f"},
        {"foo", "666f6f"},
        {"foob", "666f6f62"},
        {"fooba", "666f6f6261"},
        {"foobar", "666f6f626172"}
    };

    // Encoding:
    for (auto &test: cases)
        REQUIRE(test.text == abcd::base16Encode(std::string(test.data)));

    // Decoding:
    for (auto &test: cases)
    {
        abcd::DataChunk result;
        REQUIRE(abcd::base16Decode(result, test.text));
        REQUIRE(abcd::toString(result) == test.data);
    }
}

TEST_CASE("Bad base16 strings", "[crypto][base16]")
{
    abcd::DataChunk result;

    // Bad length:
    REQUIRE_FALSE(abcd::base16Decode(result, "123"));

    // Bad padding:
    REQUIRE_FALSE(abcd::base16Decode(result, "00=="));
    REQUIRE_FALSE(abcd::base16Decode(result, "0="));
}

TEST_CASE("RFC 4648 base32 test vectors", "[crypto][base32]")
{
    struct TestCase
    {
        const char *data;
        const char *text;
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

    // Encoding:
    for (auto &test: cases)
        REQUIRE(test.text == abcd::base32Encode(std::string(test.data)));

    // Decoding:
    for (auto &test: cases)
    {
        abcd::DataChunk result;
        REQUIRE(abcd::base32Decode(result, test.text));
        REQUIRE(abcd::toString(result) == test.data);
    }
}

TEST_CASE("Bad base32 strings", "[crypto][base32]")
{
    abcd::DataChunk result;

    // Bad length:
    REQUIRE_FALSE(abcd::base32Decode(result, "12345"));

    // Bad padding:
    REQUIRE_FALSE(abcd::base32Decode(result, "AAAAAAAA========"));
    REQUIRE_FALSE(abcd::base32Decode(result, "A======="));
    REQUIRE_FALSE(abcd::base32Decode(result, "AAA====="));
    REQUIRE_FALSE(abcd::base32Decode(result, "AAAAAA=="));

    // Illegal characters:
    REQUIRE_FALSE(abcd::base32Decode(result, "A1======"));
    REQUIRE_FALSE(abcd::base32Decode(result, "Aa======"));
}

TEST_CASE("RFC 4648 base64 test vectors", "[crypto][base64]")
{
    struct TestCase
    {
        const char *data;
        const char *text;
    };
    TestCase cases[] =
    {
        {"", ""},
        {"f", "Zg=="},
        {"fo", "Zm8="},
        {"foo", "Zm9v"},
        {"foob", "Zm9vYg=="},
        {"fooba", "Zm9vYmE="},
        {"foobar", "Zm9vYmFy"}
    };

    // Encoding:
    for (auto &test: cases)
        REQUIRE(test.text == abcd::base64Encode(std::string(test.data)));

    // Decoding:
    for (auto &test: cases)
    {
        abcd::DataChunk result;
        REQUIRE(abcd::base64Decode(result, test.text));
        REQUIRE(abcd::toString(result) == test.data);
    }
}

TEST_CASE("Unusual base64 characters", "[crypto][base64]")
{
    abcd::DataChunk result;
    REQUIRE(abcd::base64Decode(result, "+/+="));
    REQUIRE(result.size() == 2);
    REQUIRE(0xfb == result[0]);
    REQUIRE(0xff == result[1]);
}

TEST_CASE("Bad base64 strings", "[crypto][base64]")
{
    abcd::DataChunk result;

    // Bad length:
    REQUIRE_FALSE(abcd::base64Decode(result, "12345"));

    // Bad padding:
    REQUIRE_FALSE(abcd::base64Decode(result, "AAAA===="));
    REQUIRE_FALSE(abcd::base64Decode(result, "A==="));
}
