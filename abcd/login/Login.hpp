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
    const std::string &
    syncKey() const { return syncKey_; }

    /**
     * Loads the authKey (LP1) from the on-disk care package.
     */
    Status
    authKey(DataChunk &result) const;

private:
    // No mutex, since all members are immutable after init.
    // The lobby mutex can cover disk-based things like logging in and
    // changing passwords if we ever want to to protect those one day.
    const std::shared_ptr<Lobby> parent_;
    const DataChunk rootKey_;
    const DataChunk dataKey_;
    const std::string syncKey_;

    Login(Lobby &lobby, DataSlice rootKey, DataSlice dataKey, std::string syncKey);
};

// Constructors:
tABC_CC ABC_LoginCreate(std::shared_ptr<Login> &result,
                        Lobby &lobby,
                        const char *szPassword,
                        tABC_Error *pError);

} // namespace abcd

#endif
