/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/crypto/Scrypt.hpp"
#include "../abcd/crypto/Encoding.hpp"
#include "../minilibs/catch/catch.hpp"

TEST_CASE("Scrypt RFC test vectors", "[crypto][scrypt]")
{
    struct TestCase
    {
        std::string password;
        std::string salt;
        uint64_t N;
        uint32_t r;
        uint32_t p;
        size_t dklen;
        const char *result;
    };
    TestCase cases[] =
    {
        {
            "", "", 16, 1, 1, 64,
            "77d6576238657b203b19ca42c18a0497"
            "f16b4844e3074ae8dfdffa3fede21442"
            "fcd0069ded0948f8326a753a0fc81f17"
            "e8d3e0fb2e0d3628cf35e20c38d18906"
        },
#if SLOW_SCRYPT_TESTS
        {
            "password", "NaCl", 1024, 8, 16, 64,
            "fdbabe1c9d3472007856e7190d01e9fe"
            "7c6ad7cbc8237830e77376634b373162"
            "2eaf30d92e22a3886ff109279d9830da"
            "c727afb94a83ee6d8360cbdfa2cc0640"
        },
        {
            "pleaseletmein", "SodiumChloride", 16384, 8, 1, 64,
            "7023bdcb3afd7348461c06cd81fd38eb"
            "fda8fbba904f8e3ea9b543f6545da1f2"
            "d5432955613f0fcf62d49705242a9af9"
            "e61e85dc0d651e40dfcf017b45575887"
        },
        {
            "pleaseletmein", "SodiumChloride", 1048576, 8, 1, 64,
            "2101cb9b6a511aaeaddbbe09cf70f881"
            "ec568d574a2ffd4dabe5ee9820adaa47"
            "8e56fd8f4ba5d09ffa1c6d927c40f4c3"
            "37304049e8a952fbcbf45c6fa77a41a4"
        }
#else
        {
            "air", "bitz", 16, 2, 1, 64, // Fast, but not from the RFC
            "a7baec15cc38090b1ec207421105acbd"
            "ad4e046be2ac04c3ecf5c01710691496"
            "92040affcee0b7bd0798dd284ae26268"
            "b17933839588c9bf1bd2d62baddf3fbb"
        }
#endif
    };

    for (auto &test: cases)
    {
        abcd::ScryptSnrp snrp =
        {
            abcd::DataChunk(test.salt.begin(), test.salt.end()),
            test.N, test.r, test.p
        };
        abcd::DataChunk out;
        unsigned long totalTime;
        CHECK(snrp.hash(out, test.password, &totalTime, test.dklen));
        CHECK(abcd::base16Encode(out) == test.result);
    }
}
