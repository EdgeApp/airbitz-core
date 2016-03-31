/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../abcd/General.hpp"
#include "../minilibs/catch/catch.hpp"

TEST_CASE("Airbitz fee splitting", "[bitcoin][spend]")
{
    abcd::AirbitzFeeInfo info;
    info.minSatoshi = 1000;
    info.maxSatoshi = 10000;
    info.noFeeMinSatoshi = 500;
    info.rate = 0.1;

    const int amounts[] =
    {
        1, 10, 100, 1000, 10000, 100000,
        34330, 16079, 773795, 666600, 876416
    };
    for (const auto amount: amounts)
    {
        // No fee:
        REQUIRE(amount == generalAirbitzFeeSpendable(info, amount, true));

        // We must be able to spend this much:
        auto spendable = generalAirbitzFeeSpendable(info, amount, false);
        auto total1 = spendable + generalAirbitzFee(info, spendable, false);
        REQUIRE(total1 <= amount);

        // But not a satoshi more:
        spendable += 1;
        auto total2 = spendable + generalAirbitzFee(info, spendable, false);
        REQUIRE(amount < total2);
    }
}

