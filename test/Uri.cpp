/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/http/Uri.hpp"
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
