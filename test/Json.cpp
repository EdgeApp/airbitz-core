/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/json/JsonObject.hpp"
#include "../minilibs/catch/catch.hpp"

TEST_CASE("Json file classes", "[util][json]")
{
    struct TestFile:
        public abcd::JsonObject
    {
        ABC_JSON_VALUE  (MyValue,   "value",   JSON_ARRAY)
        ABC_JSON_STRING (MyString,  "string",  "default")
        ABC_JSON_NUMBER (MyNumber,  "number",  6.28)
        ABC_JSON_BOOLEAN(MyBoolean, "boolean", true)
        ABC_JSON_INTEGER(MyInteger, "integer", 42)
    };
    TestFile test;

    SECTION("empty")
    {
        REQUIRE_FALSE(test.hasMyValue());
        REQUIRE_FALSE(test.hasMyString());
        REQUIRE_FALSE(test.hasMyNumber());
        REQUIRE_FALSE(test.hasMyBoolean());
        REQUIRE_FALSE(test.hasMyInteger());
    }
    SECTION("defaults")
    {
        REQUIRE(test.getMyValue() == nullptr);
        REQUIRE(test.getMyString() == std::string("default"));
        REQUIRE(test.getMyNumber() == 6.28);
        REQUIRE(test.getMyBoolean() == true);
        REQUIRE(test.getMyInteger() == 42);
    }
    SECTION("raw")
    {
        REQUIRE(test.decode("{ \"value\": [] }"));
        REQUIRE(test.hasMyValue());
    }
    SECTION("bad raw type")
    {
        REQUIRE(test.decode("{ \"value\": {} }"));
        REQUIRE_FALSE(test.hasMyValue());
    }
    SECTION("string decode")
    {
        REQUIRE(test.decode("{ \"string\": \"value\" }"));
        REQUIRE(test.hasMyString());
        REQUIRE(test.getMyString() == std::string("value"));
    }
    SECTION("number decode")
    {
        REQUIRE(test.decode("{ \"number\": 1.1 }"));
        REQUIRE(test.hasMyNumber());
        REQUIRE(test.getMyNumber() == 1.1);
    }
    SECTION("boolean set")
    {
        REQUIRE(test.setMyBoolean(false));
        REQUIRE(test.getMyBoolean() == false);
    }
    SECTION("integer set")
    {
        REQUIRE(test.setMyInteger(65537));
        REQUIRE(test.getMyInteger() == 65537);
    }
}
