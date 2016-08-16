/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_LOGIN_HPP
#define ABCD_LOGIN_LOGIN_HPP

#include "../AccountPaths.hpp"
#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <memory>
#include <mutex>

namespace abcd {

class JsonBox;
class LoginStore;
struct LoginPackage;

/**
 * Holds the keys for a logged-in account.
 */
class Login:
    public std::enable_shared_from_this<Login>
{
public:
    LoginStore &store;
    AccountPaths paths;

    static Status
    createOffline(std::shared_ptr<Login> &result,
                  LoginStore &store, DataSlice dataKey);

    static Status
    createOnline(std::shared_ptr<Login> &result,
                 LoginStore &store, DataSlice dataKey,
                 const LoginPackage &loginPackage, JsonBox rootKeyBox);

    static Status
    createNew(std::shared_ptr<Login> &result,
              LoginStore &store, const char *password);

    /**
     * Obtains the root key for the account.
     */
    DataSlice
    rootKey() const { return rootKey_; }

    /**
     * Obtains the master key for the account.
     */
    DataSlice
    dataKey() const { return dataKey_; }

    /**
     * Obtains the data-sync key for the account.
     */
    std::string
    syncKey() const;

    /**
     * Loads the server authentication key (LP1) for the account.
     */
    DataChunk
    passwordAuth() const;

    /**
     * Used when changing the password for the login.
     */
    Status
    passwordAuthSet(DataSlice passwordAuth);

private:
    mutable std::mutex mutex_;
    const std::shared_ptr<LoginStore> parent_;

    // Keys:
    const DataChunk dataKey_;
    DataChunk rootKey_;
    DataChunk syncKey_;
    DataChunk passwordAuth_;

    Login(LoginStore &store, DataSlice dataKey);

    Status
    createNew(const char *password);

    /**
     * Unpacks the keys from the loginPackage.
     */
    Status
    loadOffline();

    Status
    loadOnline(const LoginPackage &loginPackage, JsonBox rootKeyBox);

    Status
    rootKeyUpgrade();
};

} // namespace abcd

#endif
