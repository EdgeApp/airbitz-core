/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/Context.hpp"
#include "../../abcd/login/LoginStore.hpp"
#include "../../abcd/login/Otp.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::store, OtpKeyGet, "otp-key-get",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    const OtpKey *key = session.store->otpKey();
    if (key)
        std::cout << "key: " << key->encodeBase32() << std::endl;
    else
        std::cout << "no key" << std::endl;

    return Status();
}

COMMAND(InitLevel::store, OtpKeySet, "otp-key-set",
        " <key>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto rawKey = argv[0];

    OtpKey key;
    ABC_CHECK(key.decodeBase32(rawKey));
    ABC_CHECK(session.store->otpKeySet(key));

    return Status();
}

COMMAND(InitLevel::store, OtpKeyRemove, "otp-key-remove",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    ABC_CHECK(session.store->otpKeyRemove());

    return Status();
}

COMMAND(InitLevel::login, OtpAuthGet, "otp-auth-get",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    bool enabled;
    long timeout;
    ABC_CHECK(otpAuthGet(*session.login, enabled, timeout));
    if (enabled)
        std::cout << "OTP on, timeout: " << timeout << std::endl;
    else
        std::cout << "OTP off." << std::endl;

    return Status();
}

COMMAND(InitLevel::login, OtpAuthSet, "otp-auth-set",
        " <timeout-sec>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto timeout = atol(argv[0]);

    ABC_CHECK(otpAuthSet(*session.login, timeout));

    return Status();
}

COMMAND(InitLevel::login, OtpAuthRemove, "otp-auth-remove",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    ABC_CHECK(otpAuthRemove(*session.login));
    ABC_CHECK(session.store->otpKeyRemove());

    return Status();
}

COMMAND(InitLevel::context, OtpResetGet, "otp-reset-get",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    std::list<std::string> result;
    ABC_CHECK(otpResetGet(result, gContext->paths.accountList()));
    for (auto &i: result)
        std::cout << i << std::endl;

    return Status();
}

COMMAND(InitLevel::login, OtpResetRemove, "otp-reset-remove",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    ABC_CHECK(otpResetRemove(*session.login));

    return Status();
}
