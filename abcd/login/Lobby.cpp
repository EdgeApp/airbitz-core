/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Lobby.hpp"
#include "LoginDir.hpp"
#include "LoginServer.hpp"
#include "../crypto/Encoding.hpp"
#include "../json/JsonObject.hpp"
#include "../util/FileIO.hpp"

namespace abcd {

struct OtpFile:
    public JsonObject
{
    ABC_JSON_STRING(key, "TOTP", "!bad")
};

const char otpFilename[] = "OtpKey.json";

Status
Lobby::create(std::shared_ptr<Lobby> &result, const std::string &username)
{
    std::shared_ptr<Lobby> out(new Lobby());
    ABC_CHECK(out->init(username));

    result = std::move(out);
    return Status();
}

const std::string &
Lobby::dir() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    return dir_;
}

Status
Lobby::dirCreate()
{
    std::lock_guard<std::mutex> lock(mutex_);
    ABC_CHECK_OLD(ABC_LoginDirCreate(dir_, username_.c_str(), &error));
    ABC_CHECK(otpKeySave());
    return Status();
}

Status
Lobby::otpKeySet(const OtpKey &key)
{
    std::lock_guard<std::mutex> lock(mutex_);

    otpKey_ = key;
    otpKeyOk_ = true;
    ABC_CHECK(otpKeySave());
    return Status();
}

Status
Lobby::otpKeyRemove()
{
    std::lock_guard<std::mutex> lock(mutex_);

    if (!dir_.empty())
    {
        auto filename = dir_ + otpFilename;
        ABC_CHECK_OLD(ABC_FileIODeleteFile(filename.c_str(), &error));
    }
    otpKeyOk_ = false;
    return Status();
}

Status
Lobby::fixUsername(std::string &result, const std::string &username)
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
Lobby::init(const std::string &username)
{
    // Set up identity:
    ABC_CHECK(fixUsername(username_, username));
    dir_ = loginDirFind(username_);

    // Create authId:
    // TODO: Make this lazy!
    ABC_CHECK(usernameSnrp().hash(authId_, username_));
    ABC_DebugLog("authId: %s", base16Encode(authId()).c_str());

    // Load the OTP key, if possible:
    OtpFile file;
    otpKeyOk_ = !dir_.empty() &&
        file.load(dir_ + otpFilename) &&
        otpKey_.decodeBase32(file.key());

    return Status();
}

Status
Lobby::otpKeySave()
{
    if (!dir_.empty() && otpKeyOk_)
    {
        OtpFile file;
        ABC_CHECK(file.keySet(otpKey_.encodeBase32().c_str()));
        ABC_CHECK(file.save(dir_ + otpFilename));
    }
    return Status();
}

} // namespace abcd
