/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_LOBBY_HPP
#define ABCD_LOGIN_LOBBY_HPP

#include "OtpKey.hpp"
#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <string>

namespace abcd {

class OtpKey;

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

    /**
     * Obtains the OTP key associated with this user, if any.
     */
    const OtpKey *
    otpKey() const { return otpKeyOk_? &otpKey_ : nullptr; }

    /**
     * Assigns an existing OTP key to the account.
     */
    Status
    otpKey(const OtpKey &key);

    /**
     * Removes the OTP key and deletes the file, if any.
     */
    Status
    otpKeyRemove();

    /**
     * Re-formats a username to all-lowercase,
     * checking for disallowed characters and collapsing spaces.
     */
    static Status
    fixUsername(std::string &result, const std::string &username);

private:
    std::string username_;
    std::string directory_;
    DataChunk authId_;

    bool otpKeyOk_ = false;
    OtpKey otpKey_;

    /**
     * Writes the OTP key to disk, assuming the account has a directory.
     */
    Status
    otpKeySave();
};

} // namespace abcd

#endif
