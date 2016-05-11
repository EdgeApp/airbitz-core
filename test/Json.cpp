/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/json/JsonArray.hpp"
#include "../abcd/json/JsonObject.hpp"
#include "../minilibs/catch/catch.hpp"

TEST_CASE("JsonPtr lifetime", "[util][json]")
{
    abcd::JsonPtr a(json_integer(42));
    REQUIRE(1 == a.get()->refcount);
    REQUIRE(json_is_integer(a.get()));

    SECTION("move constructor")
    {
        abcd::JsonPtr b(std::move(a));
        REQUIRE(1 == b.get()->refcount);
        REQUIRE(json_is_integer(b.get()));
        REQUIRE(!a.get());
    }
    SECTION("copy constructor")
    {
        abcd::JsonPtr b(a);
        REQUIRE(2 == b.get()->refcount);
        REQUIRE(json_is_integer(b.get()));
        REQUIRE(json_is_integer(a.get()));
    }
    SECTION("assignment operator")
    {
        abcd::JsonPtr b;
        b = a;
        REQUIRE(2 == b.get()->refcount);
        REQUIRE(json_is_integer(b.get()));
        REQUIRE(json_is_integer(a.get()));

        b = nullptr;
        REQUIRE(!b.get());
        REQUIRE(1 == a.get()->refcount);
    }
}

TEST_CASE("JsonArray manipulation", "[util][json]")
{
    abcd::JsonArray a;
    REQUIRE(json_is_array(a.get()));

    abcd::JsonPtr temp(json_integer(42));
    REQUIRE(a.append(temp));

    REQUIRE(a.get());
    REQUIRE(1 == a.size());
    REQUIRE(json_is_integer(a[0].get()));
    REQUIRE(42 == json_integer_value(a[0].get()));
}

TEST_CASE("JsonObject manipulation", "[util][json]")
{
    struct TestJson:
        public abcd::JsonObject
    {
        ABC_JSON_VALUE  (value,   "value",   JsonPtr)
        ABC_JSON_STRING (string,  "string",  "default")
        ABC_JSON_NUMBER (number,  "number",  6.28)
        ABC_JSON_BOOLEAN(boolean, "boolean", true)
        ABC_JSON_INTEGER(integer, "integer", 42)
    };
    TestJson test;

    SECTION("empty")
    {
        REQUIRE(json_is_object(test.get()));
        REQUIRE_FALSE(test.stringOk());
        REQUIRE_FALSE(test.numberOk());
        REQUIRE_FALSE(test.booleanOk());
        REQUIRE_FALSE(test.integerOk());
    }
    SECTION("defaults")
    {
        REQUIRE_FALSE(test.value());
        REQUIRE(test.string() == std::string("default"));
        REQUIRE(test.number() == 6.28);
        REQUIRE(test.boolean() == true);
        REQUIRE(test.integer() == 42);
    }
    SECTION("raw json")
    {
        REQUIRE(test.decode("{ \"value\": [] }"));
        REQUIRE(test.value());
    }
    SECTION("string decode")
    {
        REQUIRE(test.decode("{ \"string\": \"value\" }"));
        REQUIRE(test.stringOk());
        REQUIRE(test.string() == std::string("value"));
    }
    SECTION("number decode")
    {
        REQUIRE(test.decode("{ \"number\": 1.1 }"));
        REQUIRE(test.numberOk());
        REQUIRE(test.number() == 1.1);
    }
    SECTION("boolean set")
    {
        REQUIRE(test.booleanSet(false));
        REQUIRE(test.boolean() == false);
    }
    SECTION("integer set")
    {
        REQUIRE(test.integerSet(65537));
        REQUIRE(test.integer() == 65537);
    }
}
