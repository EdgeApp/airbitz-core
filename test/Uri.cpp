/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/http/Uri.hpp"
#include "../abcd/bitcoin/Text.hpp"
#include "../minilibs/catch/catch.hpp"

TEST_CASE("Basic URI handling", "[util][uri]" )
{
    const std::string test = "http://github.com/libbitcoin?good=true#nice";
    abcd::Uri uri;
    REQUIRE(uri.decode(test));

    REQUIRE(uri.authorityOk());
    REQUIRE(uri.queryOk());
    REQUIRE(uri.fragmentOk());

    REQUIRE(uri.scheme() == "http");
    REQUIRE(uri.authority() == "github.com");
    REQUIRE(uri.path() == "/libbitcoin");
    REQUIRE(uri.query() == "good=true");
    REQUIRE(uri.fragment() == "nice");

    REQUIRE(uri.encode() == test);
}

TEST_CASE("Messy URI round-tripping", "[util][uri]" )
{
    const std::string test = "TEST:%78?%79#%7a";
    abcd::Uri uri;
    REQUIRE(uri.decode(test));

    REQUIRE(!uri.authorityOk());
    REQUIRE(uri.queryOk());
    REQUIRE(uri.fragmentOk());

    REQUIRE(uri.scheme() == "test");
    REQUIRE(uri.path() == "x");
    REQUIRE(uri.query() == "y");
    REQUIRE(uri.fragment() == "z");

    REQUIRE(uri.encode() == test);
}

TEST_CASE("URI scheme tests", "[util][uri]" )
{
    abcd::Uri uri;

    SECTION("errors")
    {
        REQUIRE(!uri.decode(""));
        REQUIRE(!uri.decode(":"));
        REQUIRE(!uri.decode("1:"));
        REQUIRE(!uri.decode("%78:"));
    }
    SECTION("good scheme")
    {
        REQUIRE(uri.decode("x:"));
        REQUIRE(uri.scheme() == "x");
    }
    SECTION("double colon")
    {
        REQUIRE(uri.decode("x::"));
        REQUIRE(uri.scheme() == "x");
        REQUIRE(uri.path() == ":");
    }
}

TEST_CASE("URI non-strict parsing tests", "[util][uri]" )
{
    abcd::Uri uri;

    SECTION("strict error")
    {
        REQUIRE(!uri.decode("test:?テスト"));
    }
    SECTION("non-strict success")
    {
        REQUIRE(uri.decode("test:テスト", false));
        REQUIRE(uri.scheme() == "test");
        REQUIRE(uri.path() == "テスト");
    }
}

TEST_CASE("URI authority tests", "[util][uri]" )
{
    abcd::Uri uri;

    SECTION("no authority")
    {
        REQUIRE(uri.decode("test:/"));
        REQUIRE(!uri.authorityOk());
        REQUIRE(uri.path() == "/");
    }
    SECTION("empty authority")
    {
        REQUIRE(uri.decode("test://"));
        REQUIRE(uri.authorityOk());
        REQUIRE(uri.authority() == "");
        REQUIRE(uri.path() == "");
    }
    SECTION("extra slash")
    {
        REQUIRE(uri.decode("test:///"));
        REQUIRE(uri.authorityOk());
        REQUIRE(uri.authority() == "");
        REQUIRE(uri.path() == "/");
    }
    SECTION("double-slash path")
    {
        REQUIRE(uri.decode("test:/x//"));
        REQUIRE(!uri.authorityOk());
        REQUIRE(uri.path() == "/x//");
    }
    SECTION("authority structure characters")
    {
        REQUIRE(uri.decode("ssh://git@github.com:22/path/"));
        REQUIRE(uri.authorityOk());
        REQUIRE(uri.authority() == "git@github.com:22");
        REQUIRE(uri.path() == "/path/");
    }
}

TEST_CASE("URI query tests", "[util][uri]" )
{
    abcd::Uri uri;

    SECTION("query after fragment")
    {
        REQUIRE(uri.decode("test:#?"));
        REQUIRE(!uri.queryOk());
    }
    SECTION("messy query decoding")
    {
        REQUIRE(uri.decode("test:?&&x=y&z"));
        REQUIRE(uri.queryOk());
        REQUIRE(uri.query() == "&&x=y&z");

        auto map = uri.queryDecode();
        REQUIRE(map.end() != map.find(""));
        REQUIRE(map.end() != map.find("x"));
        REQUIRE(map.end() != map.find("z"));
        REQUIRE(map.end() == map.find("y"));

        REQUIRE(map[""] == "");
        REQUIRE(map["x"] == "y");
        REQUIRE(map["z"] == "");
    }
}

TEST_CASE("URI fragment tests", "[util][uri]" )
{
    abcd::Uri uri;

    SECTION("no fragment")
    {
        REQUIRE(uri.decode("test:?"));
        REQUIRE(!uri.fragmentOk());
    }
    SECTION("empty fragment")
    {
        REQUIRE(uri.decode("test:#"));
        REQUIRE(uri.fragmentOk());
        REQUIRE(uri.fragment() == "");
    }
    SECTION("query after fragment")
    {
        REQUIRE(uri.decode("test:#?"));
        REQUIRE(uri.fragmentOk());
        REQUIRE(uri.fragment() == "?");
    }
}

TEST_CASE("URI encoding test", "[util][uri]" )
{
    abcd::Uri uri;
    uri.schemeSet("test");
    uri.authoritySet("user@hostname");
    uri.pathSet("/some/path/?/#");
    uri.querySet("tacos=yummy");
    uri.fragmentSet("good evening");

    REQUIRE(uri.encode() ==
            "test://user@hostname/some/path/%3F/%23?tacos=yummy#good%20evening");

    uri.authorityRemove();
    uri.queryRemove();
    uri.fragmentRemove();

    REQUIRE(uri.encode() ==
            "test:/some/path/%3F/%23");
}

TEST_CASE("ParsedUri test", "[bitcoin][uri]")
{
    abcd::ParsedUri uri;

    SECTION("airbitz URI")
    {
        auto text = "airbitz://bitcoin/113Pfw4sFqN1T5kXUnKbqZHMJHN9oyjtgD?amount=0.1";
        REQUIRE(parseUri(uri, text));
        REQUIRE(uri.address == "113Pfw4sFqN1T5kXUnKbqZHMJHN9oyjtgD");
        REQUIRE(uri.amountSatoshi == 10000000u);
    }
    SECTION("bitcoin URI")
    {
        auto text = "bitcoin:113Pfw4sFqN1T5kXUnKbqZHMJHN9oyjtgD?amount=0.1";
        REQUIRE(parseUri(uri, text));
        REQUIRE(uri.address == "113Pfw4sFqN1T5kXUnKbqZHMJHN9oyjtgD");
        REQUIRE(uri.amountSatoshi == 10000000u);
    }
    SECTION("payment request URI")
    {
        auto text =
            "bitcoin:?r=https://airbitz.co&label=l&message=m m&category=c&ret=r";
        REQUIRE(parseUri(uri, text));
        REQUIRE(uri.address.empty());
        REQUIRE(uri.paymentProto == "https://airbitz.co");
        REQUIRE(uri.label == "l");
        REQUIRE(uri.message == "m m");
        REQUIRE(uri.category == "c");
        REQUIRE(uri.ret == "r");
    }
    SECTION("bitid URI")
    {
        auto text = "bitid://bitid.bitcoin.blue/callback?x=fbc3ac5e2615dece&u=1";
        REQUIRE(parseUri(uri, text));
        REQUIRE(uri.bitidUri == text);
    }
    SECTION("OTP URI")
    {
        auto text =
            "otpauth://totp/Example:alice@google.com?secret=JBSWY3DPEHPK3PXP&issuer=Example";
        REQUIRE(parseUri(uri, text));
        REQUIRE(uri.otpKey == "JBSWY3DPEHPK3PXP");
        REQUIRE(uri.label == "Example:alice@google.com");
    }
    SECTION("wif")
    {
        auto text = "KzuvBLcUQsfKcjHRhoe7D8UfzjLRsjB14AppLwSsb8uTdKHH45vM";
        REQUIRE(parseUri(uri, text));
        REQUIRE(uri.wif == text);
        REQUIRE(uri.address == "18LVsfoGUPWvK7b8L3WdgmDt4katk8nWf6");
    }
    SECTION("minikey")
    {
        auto text = "S4b3N3oGqDqR5jNuxEvDwf";
        REQUIRE(parseUri(uri, text));
        REQUIRE(uri.wif == "5HueCGU8rMjxEXxiPuD5BDku4MkFqeZyd4dZ1jvhTVqvbTLvyTJ");
        REQUIRE(uri.address == "1GAehh7TsJAHuUAeKZcXf5CnwuGuGgyX2S");
    }
}
