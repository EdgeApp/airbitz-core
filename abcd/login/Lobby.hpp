/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_LOBBY_HPP
#define ABCD_LOGIN_LOBBY_HPP

#include "../crypto/OtpKey.hpp"
#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <memory>
#include <mutex>

namespace abcd {

class OtpKey;

/**
 * The lobby object contains the account data that is knowable from just
 * the username, without logging in.
 */
class Lobby:
    public std::enable_shared_from_this<Lobby>
{
public:
    static Status
    create(std::shared_ptr<Lobby> &result, const std::string &username);

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
    dir() const;

    std::string carePackageName() { return dir() + "CarePackage.json"; }
    std::string loginPackageName() { return dir() + "LoginPackage.json"; }
    std::string rootKeyPath() { return dir() + "RootKey.json"; }

    /**
     * Creates a directory for the account if one does not already exist.
     */
    Status
    dirCreate();

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
    otpKeySet(const OtpKey &key);

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
    mutable std::mutex mutex_;
    std::string username_;
    std::string dir_;
    DataChunk authId_;

    bool otpKeyOk_ = false;
    OtpKey otpKey_;

    Lobby() {};

    Status
    init(const std::string &username);

    /**
     * Writes the OTP key to disk, assuming the account has a directory.
     * The caller must already be holding the mutex.
     */
    Status
    otpKeySave();
};

} // namespace abcd

#endif
