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
#include "../util/Sync.hpp"

namespace abcd {

class Lobby;
typedef struct sABC_LoginPackage tABC_LoginPackage;

/**
 * Holds the keys for a logged-in account.
 */
class Login
{
public:
    Login(Lobby *lobby, DataSlice dataKey);

    /**
     * Prepares the Login object for use.
     */
    Status
    init(tABC_LoginPackage *package);

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
    Lobby *lobby_;
    DataChunk dataKey_;
    std::string syncKey_;
};

typedef Login tABC_Login;

// Constructors:
tABC_CC ABC_LoginCreate(Login *&result,
                        Lobby *lobby,
                        const char *szPassword,
                        tABC_Error *pError);

// Read accessors:
tABC_CC ABC_LoginGetSyncKeys(Login &login,
                             tABC_SyncKeys **ppKeys,
                             tABC_Error *pError);

tABC_CC ABC_LoginGetServerKeys(Login &login,
                               tABC_U08Buf *pL1,
                               tABC_U08Buf *pLP1,
                               tABC_Error *pError);

} // namespace abcd

#endif
