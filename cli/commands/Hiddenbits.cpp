/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/bitcoin/Text.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::context, CliGenerateHiddenbits, "hiddenbits-generate",
        " [<count>]")
{
    if (0 != argc && 1 != argc)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    long count = 1;
    if (1 == argc)
        count = atol(argv[0]);

    for (long i = 0; i < count; ++i)
    {
        std::string key;
        std::string address;
        ABC_CHECK(hbitsCreate(key, address));
        std::cout << key << " " << address << std::endl;
    }

    return Status();
}
