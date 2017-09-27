/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/bitcoin/Utility.hpp"
#include "../abcd/crypto/Encoding.hpp"
#include "../abcd/json/JsonBox.hpp"
#include "../minilibs/catch/catch.hpp"

#include <iostream>

static const char rawTxHex[] =
    "0100000000010170632233be35f8b6deb07e0e13d31cd6efa03b5a7e05afe619e5017acda23b6400000000171600145888c0ee06ce9ceaebe253d67e7e547f8bb3db05ffffffff0132430300000000001976a9143801b8cff780ca0853df97d247ab64980cc0638e88ac0247304402203470c6871ae67ae74d6eced94d57f4970e10a52523d329991fa21caa7125876d0220628ae0c349333d7636e6a14ce1e1defd2459b3b5d44b1a4fe96593e00da321c40121033f0463711a8815af06cbfc44d73ce8f5da613e81ddd83413a4af08d5b3ff2f8000000000";

TEST_CASE("Decode segwit transaction", "[bitcoin]")
{
    abcd::DataChunk rawTx;
    abcd::base16Decode(rawTx, rawTxHex);

    bc::transaction_type result;
    CHECK(abcd::decodeTx(result, rawTx));
    REQUIRE(result.inputs.size() == 1);
    REQUIRE(result.outputs.size() == 1);
    REQUIRE(result.locktime == 0);
}
