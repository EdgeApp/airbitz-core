/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/crypto/Crypto.hpp"
#include "../abcd/crypto/Encoding.hpp"
#include "../abcd/json/JsonPtr.hpp"
#include "../minilibs/catch/catch.hpp"

// sha256("Satoshi"):
static const char keyHex[] =
    "002688cc350a5333a87fa622eacec626c3d1c0ebf9f3793de3885fa254d7e393";

TEST_CASE("File name", "[crypto]")
{
    const std::string name("1PeChFbhxDD9NLbU21DfD55aQBC4ZTR3tE");
    const std::string key("Satoshi");
    CHECK(abcd::cryptoFilename(key, name) ==
        "5vJNMWZ68tsp2HJa1AfMhZpcpU9Wm9ccEw7cTwvARHXh");
}

TEST_CASE("Decryption", "[crypto][encryption]")
{
    tABC_Error error;
    abcd::DataChunk key;
    abcd::base16Decode(key, keyHex);
    abcd::JsonPtr package;
    package.decode(
        "{"
        "\"data_base64\": "
          "\"X08Snnou2PrMW21ZNyJo5C8StDjTNgMtuEoAJL5bJ6LDPdZGQLhjaUMetOknaPYn"
          "mfBCHNQ3ApqmE922Hkp30vdxzXBloopfPLJKdYwQxURYNbiL4TvNakP7i0bnTlIsR7"
          "bj1q/65ZyJOW1HyOKV/tmXCf56Fhe3Hcmb/ebsBF72FZr3jX5pkSBO+angK15IlCIi"
          "em1kPi6QmzyFtMB11i0GTjSS67tLrWkGIqAmik+bGqy7WtQgfMRxQNNOxePPSHHp09"
          "431Ogrc9egY3txnBN2FKnfEM/0Wa/zLWKCVQXCGhmrTx1tmf4HouNDOnnCgkRWJYs8"
          "FJdrDP8NZy4Fkzs7FoH7RIaUiOvosNKMil1CBknKremP6ohK7SMLGoOHpv+bCgTXcA"
          "eB3P4Slx3iy+RywTSLb3yh+HDo6bwt+vhujP0RkUamI5523bwz3/7vLO8BzyF6WX0B"
          "y2s4gvMdFQ==\","
        "\"encryptionType\": 0,"
        "\"iv_hex\": \"96a4cd52670c13df9712fdc1b564d44b\""
        "}");

    abcd::AutoU08Buf data;
    CHECK(ABC_CC_Ok == ABC_CryptoDecryptJSONObject(
        package.get(), abcd::toU08Buf(key), &data, &error));
    CHECK(abcd::toString(data) == "payload");
}

TEST_CASE("Encryption round-trip", "[crypto][encryption]")
{
    tABC_Error error;
    abcd::DataChunk key;
    abcd::base16Decode(key, keyHex);
    const std::string payload("payload");

    json_t *json = nullptr;
    CHECK(ABC_CC_Ok == ABC_CryptoEncryptJSONObject(
        abcd::toU08Buf(payload), abcd::toU08Buf(key),
        abcd::ABC_CryptoType_AES256, &json, &error));

    abcd::AutoU08Buf data;
    CHECK(ABC_CC_Ok == ABC_CryptoDecryptJSONObject(
        json, abcd::toU08Buf(key), &data, &error));
    CHECK(abcd::toString(data) == payload);

    if (json)
        json_decref(json);
}
