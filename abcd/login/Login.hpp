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

class JsonPtr;
class LoginStore;
struct LoginPackage;
class LoginReplyJson;

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
                 LoginStore &store, DataSlice dataKey, LoginReplyJson loginJson);

    static Status
    createNew(std::shared_ptr<Login> &result,
              LoginStore &store, const char *password);

    /**
     * Fetches the latest login resources from the server.
     */
    Status
    update();

    /**
     * Obtains the root key for the account.
     */
    DataSlice
    rootKey() const { return rootKey_; }

    /**
     * Obtains the encryption key for the login data.
     */
    DataSlice
    dataKey() const { return dataKey_; }

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

    /**
     * Finds (or creates) a repo with a particular type.
     */
    Status
    repoFind(JsonPtr &result, const std::string &type, bool create=true);

    /**
     * Forms a login tree suitable for edge login.
     * Will prune away irrelevant logins and keys,
     * and will create a new login object if necessary.
     */
    Status
    makeEdgeLogin(JsonPtr &result, const std::string &appId,
                  const std::string &pin);

private:
    mutable std::mutex mutex_;
    const std::shared_ptr<LoginStore> parent_;

    // Keys:
    const DataChunk dataKey_;
    DataChunk rootKey_;
    DataChunk passwordAuth_;

    Login(LoginStore &store, DataSlice dataKey);

    Status
    makeEdgeLoginLocal(JsonPtr &result, const std::string &appId);

    Status
    createNew(const char *password);

    /**
     * Unpacks the keys from the loginPackage.
     */
    Status
    loadOffline();

    Status
    loadOnline(LoginReplyJson loginJson);

    Status
    rootKeyUpgrade();
};

} // namespace abcd

#endif
