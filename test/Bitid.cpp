/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/login/Bitid.hpp"
#include "../abcd/http/Uri.hpp"
#include "../minilibs/catch/catch.hpp"

TEST_CASE("BitID callback tests", "[login][bitid]")
{
    const std::string path = "bitid.bitcoin.blue/callback";
    abcd::Uri result;

    SECTION("no authority")
    {
        REQUIRE(abcd::bitidCallback(result, "bitid:" + path + "?x=1"));
        REQUIRE(result.encode() == "https://" + path);
    }
    SECTION("normal")
    {
        REQUIRE(abcd::bitidCallback(result, "bitid://" + path + "?x=1"));
        REQUIRE(result.encode() == "https://" + path);
    }
    SECTION("no https")
    {
        REQUIRE(abcd::bitidCallback(result, "bitid://" + path + "?x=1&u=1"));
        REQUIRE(result.encode() == "http://" + path);
    }
}

TEST_CASE("BitID key derivation", "[login][bitid]")
{
    bc::word_list mnemonic = {
        "inhale", "praise", "target", "steak", "garlic", "cricket",
        "paper", "better", "evil", "almost", "sadness", "crawl",
        "city", "banner", "amused", "fringe", "fox", "insect",
        "roast", "aunt", "prefer", "hollow", "basic", "ladder"
    };
    const auto signature = abcd::bitidSign(bc::decode_mnemonic(mnemonic),
        "test", "http://bitid.bitcoin.blue/callback", 0);
    REQUIRE(signature.address == "1J34vj4wowwPYafbeibZGht3zy3qERoUM1");
}
