/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "LoginStore.hpp"
#include "LoginPackages.hpp"
#include "server/LoginServer.hpp"
#include "../Context.hpp"
#include "../crypto/Encoding.hpp"
#include "../util/Debug.hpp"
#include "../util/FileIO.hpp"

namespace abcd {

struct OtpFile:
    public JsonObject
{
    ABC_JSON_STRING(key, "TOTP", "!bad")
};

Status
LoginStore::create(std::shared_ptr<LoginStore> &result,
                   const std::string &username)
{
    std::shared_ptr<LoginStore> out(new LoginStore());
    ABC_CHECK(out->init(username));

    result = std::move(out);
    return Status();
}

Status
LoginStore::paths(AccountPaths &result, bool create)
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!paths_.ok())
    {
        if (!create)
            return ABC_ERROR(ABC_CC_FileDoesNotExist, "No account directory");

        ABC_CHECK(gContext->paths.accountDirNew(paths_, username_));
        ABC_CHECK(otpKeySave());
    }

    result = paths_;
    return Status();
}

Status
LoginStore::otpKeySet(const OtpKey &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    otpKey_ = key;
    otpKeyOk_ = true;
    ABC_CHECK(otpKeySave());
    return Status();
}

Status
LoginStore::otpKeyRemove()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (paths_.ok())
    {
        ABC_CHECK(fileDelete(paths_.otpKeyPath()));
    }
    otpKeyOk_ = false;
    return Status();
}

Status
LoginStore::fixUsername(std::string &result, const std::string &username)
{
    std::string out;
    out.reserve(username.size());

    // Collapse leading & internal spaces:
    bool space = true;
    for (auto c: username)
    {
        if (isspace(c))
        {
            // Only write a space on the no-space -> space transition:
            if (!space)
                out += ' ';
            space = true;
        }
        else
        {
            out += c;
            space = false;
        }
    }

    // Stomp trailing space, if any:
    if (out.size() && out.back() == ' ')
        out.pop_back();

    // Scan for bad characters, and make lowercase:
    for (auto &c: out)
    {
        if (c < ' ' || '~' < c)
            return ABC_ERROR(ABC_CC_NotSupported, "Bad username");
        if ('A' <= c && c <= 'Z')
            c = c - 'A' + 'a';
    }

    result = std::move(out);
    return Status();
}

Status
LoginStore::init(const std::string &username)
{
    // Set up identity:
    ABC_CHECK(fixUsername(username_, username));

    // Failure is acceptable:
    gContext->paths.accountDir(paths_, username_);

    // Create userId:
    ABC_CHECK(usernameSnrp().hash(userId_, username_));
    ABC_DebugLog("userId: %s", base64Encode(userId()).c_str());

    // Load the OTP key, if possible:
    OtpFile file;
    otpKeyOk_ = paths_.ok() &&
                file.load(paths_.otpKeyPath()) &&
                otpKey_.decodeBase32(file.key());

    return Status();
}

Status
LoginStore::otpKeySave()
{
    if (paths_.ok() && otpKeyOk_)
    {
        OtpFile file;
        ABC_CHECK(file.keySet(otpKey_.encodeBase32()));
        ABC_CHECK(file.save(paths_.otpKeyPath()));
    }
    return Status();
}

} // namespace abcd
