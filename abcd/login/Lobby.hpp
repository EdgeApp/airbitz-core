/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_LOBBY_HPP
#define ABCD_LOGIN_LOBBY_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <string>

namespace abcd {

/**
 * The lobby object contains the account data that is knowable from just
 * the username, without logging in.
 */
class Lobby
{
public:
    /**
     * Prepares the Lobby object for use.
     * This call must succeed before object is usable.
     */
    Status
    init(const std::string &username);

    /**
     * Obtains the normalized username for this account.
     */
    const std::string &
    username() const { return username_; }

    /**
     * Returns the account's directory name.
     * The string will be empty if the directory does not exist.
     */
    const std::string &
    directory() const { return directory_; }

    /**
     * Creates a directory for the account if one does not already exist.
     */
    Status
    createDirectory();

    /**
     * Obtains the hashed username used to authenticate with the server,
     * formerly known an L1.
     */
    DataSlice
    authId() const { return authId_; }

private:
    std::string username_;
    std::string directory_;
    DataChunk authId_;
};

} // namespace abcd

#endif
