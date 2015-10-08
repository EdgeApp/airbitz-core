/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_LOGIN_HPP
#define ABCD_LOGIN_LOGIN_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <memory>
#include <mutex>

namespace abcd {

class Lobby;
struct LoginPackage;

/**
 * Holds the keys for a logged-in account.
 */
class Login:
    public std::enable_shared_from_this<Login>
{
public:
    Lobby &lobby;

    static Status
    create(std::shared_ptr<Login> &result, Lobby &lobby, DataSlice dataKey,
        const LoginPackage &loginPackage);

    static Status
    createNew(std::shared_ptr<Login> &result, Lobby &lobby,
        const char *password);

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
    authKey() const;

    Status
    authKeySet(DataSlice authKey);

private:
    mutable std::mutex mutex_;
    const std::shared_ptr<Lobby> parent_;

    // Keys:
    const DataChunk dataKey_;
    DataChunk rootKey_;
    DataChunk syncKey_;
    DataChunk authKey_;

    Login(Lobby &lobby, DataSlice dataKey);

    Status
    createNew(const char *password);

    Status
    loadKeys(const LoginPackage &loginPackage);
};

} // namespace abcd

#endif
