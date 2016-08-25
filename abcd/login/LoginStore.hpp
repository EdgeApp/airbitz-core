/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_LOGIN_STORE_HPP
#define ABCD_LOGIN_LOGIN_STORE_HPP

#include "../AccountPaths.hpp"
#include "../crypto/OtpKey.hpp"
#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <memory>
#include <mutex>

namespace abcd {

class OtpKey;

/**
 * The store object contains the account data that is knowable from just
 * the username, without logging in.
 */
class LoginStore:
    public std::enable_shared_from_this<LoginStore>
{
public:
    static Status
    create(std::shared_ptr<LoginStore> &result,
           const std::string &username);

    /**
     * Obtains the normalized username for this account.
     */
    const std::string &
    username() const { return username_; }

    /**
     * Obtains the paths object giving the file locations within the account.
     * @param create true to create the directory if it does not exist.
     */
    Status
    paths(AccountPaths &result, bool create=false);

    /**
     * Obtains the hashed username used to authenticate with the server,
     * formerly known an L1.
     */
    DataSlice
    userId() const { return userId_; }

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
    AccountPaths paths_;
    DataChunk userId_;

    bool otpKeyOk_ = false;
    OtpKey otpKey_;

    LoginStore() {};

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
