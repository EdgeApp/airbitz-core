/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "AuthJson.hpp"
#include "../Login.hpp"
#include "../LoginStore.hpp"
#include "../../crypto/Encoding.hpp"
#include "../../crypto/OtpKey.hpp"

namespace abcd {

Status
AuthJson::loginSet(const Login &login)
{
    auto otpKey = login.store.otpKey();
    if (otpKey)
        ABC_CHECK(otpSet(otpKey->totp()));
    ABC_CHECK(userIdSet(base64Encode(login.store.userId())));
    ABC_CHECK(passwordAuthSet(base64Encode(login.passwordAuth())));

    return Status();
}

} // namespace abcd
