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
class Login
{
public:
    Login(std::shared_ptr<Lobby> lobby, DataSlice dataKey);

    /**
     * Prepares the Login object for use.
     */
    Status
    init(const LoginPackage &loginPackage);

    /**
     * Obtains a reference to the lobby object associated with this account.
     */
    Lobby &
    lobby() const { return *lobby_; }

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
     * Returns the account's sync directory name.
     */
    std::string
    syncDir() const;

    /**
     * If the sync dir doesn't exist, create and sync it.
     */
    Status
    syncDirCreate() const;

private:
    // No mutex, since all members are immutable after init.
    // The lobby mutext can cover disk-based things like logging in and
    // changing passwords if we ever want to to protect those one day.
    std::shared_ptr<Lobby> lobby_;
    DataChunk dataKey_;
    std::string syncKey_;
};

// Constructors:
tABC_CC ABC_LoginCreate(std::shared_ptr<Login> &result,
                        std::shared_ptr<Lobby> lobby,
                        const char *szPassword,
                        tABC_Error *pError);

// Read accessors:
tABC_CC ABC_LoginGetServerKeys(const Login &login,
                               tABC_U08Buf *pL1,
                               tABC_U08Buf *pLP1,
                               tABC_Error *pError);

} // namespace abcd

#endif
