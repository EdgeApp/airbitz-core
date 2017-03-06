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
AuthJson::otpSet(const LoginStore &store)
{
    auto otpKey = store.otpKey();
    if (otpKey)
        ABC_CHECK(otpSet(otpKey->totp()));

    return Status();
}

Status
AuthJson::userIdSet(const LoginStore &store)
{
    ABC_CHECK(userIdSet(base64Encode(store.userId())));

    return Status();
}

Status
AuthJson::passwordSet(const LoginStore &store, DataSlice passwordAuth)
{
    ABC_CHECK(otpSet(store));
    ABC_CHECK(userIdSet(store));
    ABC_CHECK(passwordAuthSet(base64Encode(passwordAuth)));

    return Status();
}

Status
AuthJson::pin2Set(const LoginStore &store, DataSlice pin2Id,
                  DataSlice pin2Auth)
{
    ABC_CHECK(otpSet(store));
    ABC_CHECK(pin2IdSet(base64Encode(pin2Id)));
    ABC_CHECK(pin2AuthSet(base64Encode(pin2Auth)));

    return Status();
}

Status
AuthJson::recoverySet(const LoginStore &store, DataSlice recoveryAuth)
{
    ABC_CHECK(otpSet(store));
    ABC_CHECK(userIdSet(store));
    ABC_CHECK(recoveryAuthSet(base64Encode(recoveryAuth)));

    return Status();
}

Status
AuthJson::recovery2Set(const LoginStore &store, DataSlice recovery2Id)
{
    ABC_CHECK(recovery2IdSet(base64Encode(recovery2Id)));

    return Status();
}

Status
AuthJson::recovery2Set(const LoginStore &store, DataSlice recovery2Id,
                       JsonPtr recovery2Auth)
{
    ABC_CHECK(otpSet(store));
    ABC_CHECK(recovery2IdSet(base64Encode(recovery2Id)));
    ABC_CHECK(recovery2AuthSet(recovery2Auth));

    return Status();
}

Status
AuthJson::loginSet(const Login &login)
{
    ABC_CHECK(passwordSet(login.store, login.passwordAuth()));

    return Status();
}

} // namespace abcd
