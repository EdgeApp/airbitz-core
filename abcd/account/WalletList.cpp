/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "WalletList.hpp"
#include "Account.hpp"
#include "../json/JsonObject.hpp"
#include "../login/Login.hpp"
#include "../util/FileIO.hpp"
#include <dirent.h>
#include <string.h>

namespace abcd {

struct WalletJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(WalletJson, JsonObject)
    ABC_JSON_INTEGER(sort,      "SortIndex",    0)
    ABC_JSON_BOOLEAN(archived,  "Archived",     false)
    // There are other keys, but the wallet itself handles those.
};

WalletList::WalletList(const Account &account):
    account_(account),
    dir_(account.dir() + "Wallets/")
{}

Status
WalletList::load()
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Step 1: reload any wallets we already have:
    for (auto &wallet: wallets_)
        ABC_CHECK(wallet.second.load(path(wallet.first),
                                     account_.dataKey()));

    // Step 2: scan the directory for new wallets:
    DIR *dir = opendir(dir_.c_str());
    if (!dir)
        return Status(); // No directory, so no wallets

    struct dirent *de;
    while (nullptr != (de = readdir(dir)))
    {
        if (!fileIsJson(de->d_name))
            continue;

        // TODO: Be sure the file has been synced!

        // Skip stuff we already have:
        std::string id(de->d_name, de->d_name + strlen(de->d_name) - 5);
        if (wallets_.end() != wallets_.find(id))
            continue;

        // Try to load the wallet:
        JsonPtr json;
        if (json.load(dir_ + de->d_name, account_.dataKey()))
            wallets_[id] = std::move(json);
    }

    closedir(dir);
    return Status();
}

std::list<std::string>
WalletList::list() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    std::list<std::string> out;
    for (const auto &wallet: wallets_)
        out.push_back(wallet.first);

    auto compare = [this](const std::string &a, const std::string &b)
    {
        WalletJson jsonA(wallets_.find(a)->second);
        WalletJson jsonB(wallets_.find(b)->second);
        return jsonA.sort() < jsonB.sort();
    };
    out.sort(compare);
    return out;
}

Status
WalletList::reorder(const std::string &id, unsigned index)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto wallet = wallets_.find(id);
    if (wallet == wallets_.end())
        return ABC_ERROR(ABC_CC_InvalidWalletID, "No such wallet");

    WalletJson json(wallet->second);
    ABC_CHECK(json.sortSet(index));
    ABC_CHECK(json.save(path(id), account_.dataKey()));
    return Status();
}

Status
WalletList::insert(const std::string &id, const JsonPtr &keys)
{
    WalletJson json(keys);
    ABC_CHECK(json.sortSet(wallets_.size()));
    ABC_CHECK(json.archivedSet(false));
    ABC_CHECK(fileEnsureDir(dir_));
    ABC_CHECK(json.save(path(id), account_.dataKey()));

    // TODO: Don't add the wallet until the sync has finished!
    std::lock_guard<std::mutex> lock(mutex_);
    wallets_[id] = json;

    return Status();
}

Status
WalletList::remove(const std::string &id)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto wallet = wallets_.find(id);
    if (wallet == wallets_.end())
        return ABC_ERROR(ABC_CC_InvalidWalletID, "No such wallet");
    wallets_.erase(wallet);

    ABC_CHECK(fileDelete(path(id)));

    return Status();
}

Status
WalletList::json(JsonPtr &result, const std::string &id) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto wallet = wallets_.find(id);
    if (wallet == wallets_.end())
        return ABC_ERROR(ABC_CC_InvalidWalletID, "No such wallet");

    result = wallet->second.clone();
    return Status();
}

Status
WalletList::archived(bool &result, const std::string &id) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto wallet = wallets_.find(id);
    if (wallet == wallets_.end())
        return ABC_ERROR(ABC_CC_InvalidWalletID, "No such wallet");

    WalletJson json(wallet->second);
    result = json.archived();
    return Status();
}

Status
WalletList::archivedSet(const std::string &id, bool archived)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto wallet = wallets_.find(id);
    if (wallet == wallets_.end())
        return ABC_ERROR(ABC_CC_InvalidWalletID, "No such wallet");

    WalletJson json(wallet->second);
    ABC_CHECK(json.archivedSet(archived));
    ABC_CHECK(json.save(path(id), account_.dataKey()));
    return Status();
}

std::string
WalletList::path(const std::string &id) const
{
    return dir_ + id + ".json";
}

} // namespace abcd
