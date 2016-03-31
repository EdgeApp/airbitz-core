/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Testnet.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

bool isTestnet()
{
    bc::payment_address foo;
    bc::set_public_key_hash(foo, bc::null_short_hash);
    return foo.version() != 0x00;
}

uint8_t pubkeyVersion()
{
    if (isTestnet())
        return 0x6f;
    return 0x00;
}

uint8_t scriptVersion()
{
    if (isTestnet())
        return 0xc4;
    return 0x05;
}

} // namespace abcd
