/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Otp.hpp"
#include "../abcd/util/Util.hpp"
#include "../src/ABC.h"
#include <iostream>

using namespace abcd;

Status
otpKeyGet(int argc, char *argv[])
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-key-get <user>");

    AutoString key;
    ABC_CHECK_OLD(ABC_OtpKeyGet(argv[0], &key.get(), &error));
    std::cout << "key: " << key.get() << std::endl;

    return Status();
}

Status
otpKeySet(int argc, char *argv[])
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-key-set <user> <key>");

    ABC_CHECK_OLD(ABC_OtpKeySet(argv[0], argv[1], &error));

    return Status();
}

Status
otpKeyRemove(int argc, char *argv[])
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-key-remove <user>");

    ABC_CHECK_OLD(ABC_OtpKeyRemove(argv[0], &error));

    return Status();
}
