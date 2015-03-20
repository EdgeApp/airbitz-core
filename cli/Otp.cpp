/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Command.hpp"
#include "../abcd/util/Util.hpp"
#include "../src/ABC.h"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::lobby, OtpKeyGet, "otp-key-get")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-key-get <user>");

    AutoString key;
    ABC_CHECK_OLD(ABC_OtpKeyGet(argv[0], &key.get(), &error));
    std::cout << "key: " << key.get() << std::endl;

    return Status();
}

COMMAND(InitLevel::lobby, OtpKeySet, "otp-key-set")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-key-set <user> <key>");

    ABC_CHECK_OLD(ABC_OtpKeySet(argv[0], argv[1], &error));

    return Status();
}

COMMAND(InitLevel::lobby, OtpKeyRemove, "otp-key-remove")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-key-remove <user>");

    ABC_CHECK_OLD(ABC_OtpKeyRemove(argv[0], &error));

    return Status();
}

COMMAND(InitLevel::login, OtpAuthGet, "otp-auth-get")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-auth-get <user> <pass>");

    bool enabled;
    long timeout;
    ABC_CHECK_OLD(ABC_OtpAuthGet(argv[0], argv[1], &enabled, &timeout, &error));
    if (enabled)
        std::cout << "OTP on, timeout: " << timeout << std::endl;
    else
        std::cout << "OTP off." << std::endl;

    return Status();
}

COMMAND(InitLevel::login, OtpAuthSet, "otp-auth-set")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-auth-set <user> <pass> <timeout-sec>");

    ABC_CHECK_OLD(ABC_OtpAuthSet(argv[0], argv[1], atol(argv[2]), &error));

    return Status();
}

COMMAND(InitLevel::login, OtpAuthRemove, "otp-auth-remove")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-auth-remove <user> <pass>");

    ABC_CHECK_OLD(ABC_OtpAuthRemove(argv[0], argv[1], &error));
    ABC_CHECK_OLD(ABC_OtpKeyRemove(argv[0], &error));

    return Status();
}

COMMAND(InitLevel::context, OtpResetGet, "otp-reset-get")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-reset-get");

    AutoString names;
    ABC_CHECK_OLD(ABC_OtpResetGet(&names.get(), &error));
    std::cout << names.get() << std::endl;

    return Status();
}

COMMAND(InitLevel::login, OtpResetRemove, "otp-reset-remove")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, "usage: ... otp-reset-remove <user> <pass>");

    ABC_CHECK_OLD(ABC_OtpResetRemove(argv[0], argv[1], &error));

    return Status();
}
