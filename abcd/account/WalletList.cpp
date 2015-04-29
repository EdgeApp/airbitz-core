/*
 * Copyright (c) 2014, AirBitz, Inc.
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
        ABC_CHECK(wallet.second.load(filename(wallet.first),
            account_.login().dataKey()));

    // Step 2: scan the directory for new wallets:
    DIR *dir = opendir(dir_.c_str());
    if (!dir)
        return Status(); // No directory, so no wallets

    struct dirent *de;
    while (nullptr != (de = readdir(dir)))
    {
        // Skip hidden files:
        std::string name = de->d_name;
        if ('.' == name[0])
            continue;

        // Skip non-json files:
        if (name.size() < 5 || !std::equal(name.end() - 5, name.end(), ".json"))
            continue;
        name.erase(name.end() - 5, name.end());

        // TODO: Be sure the file has been synced!

        // Skip stuff we already have:
        if (wallets_.end() != wallets_.find(name))
            continue;

        // Try to load the wallet:
        JsonPtr json;
        if (json.load(filename(name), account_.login().dataKey()))
            wallets_[name] = std::move(json);
    }

    closedir(dir);
    return Status();
}

std::list<WalletList::Item>
WalletList::list() const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::list<Item> out;

    for (const auto &wallet: wallets_)
    {
        WalletJson json(wallet.second);
        out.push_back(Item{wallet.first, json.archived()});
    }

    auto compare = [this](const Item &a, const Item &b)
    {
        WalletJson jsonA(wallets_.find(a.id)->second);
        WalletJson jsonB(wallets_.find(b.id)->second);
        return jsonA.sort() < jsonB.sort();
    };
    out.sort(compare);
    return out;
}

Status
WalletList::json(JsonPtr &result, const std::string &id) const
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto wallet = wallets_.find(id);
    if (wallet == wallets_.end())
        return ABC_ERROR(ABC_CC_InvalidWalletID, "No such wallet");

    result = wallet->second;
    return Status();
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
    ABC_CHECK(json.save(filename(id), account_.login().dataKey()));
    return Status();
}

Status
WalletList::archive(const std::string &id, bool archived)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto wallet = wallets_.find(id);
    if (wallet == wallets_.end())
        return ABC_ERROR(ABC_CC_InvalidWalletID, "No such wallet");

    WalletJson json(wallet->second);
    ABC_CHECK(json.archivedSet(archived));
    ABC_CHECK(json.save(filename(id), account_.login().dataKey()));
    return Status();
}

Status
WalletList::insert(const std::string &id, const JsonPtr &keys)
{
    WalletJson json(keys);
    ABC_CHECK(json.sortSet(wallets_.size()));
    ABC_CHECK(json.archivedSet(false));
    ABC_CHECK(fileEnsureDir(dir_));
    ABC_CHECK(json.save(filename(id), account_.login().dataKey()));

    // TODO: Don't add the wallet until the sync has finished!
    std::lock_guard<std::mutex> lock(mutex_);
    wallets_[id] = json;

    return Status();
}

std::string
WalletList::filename(const std::string &name) const
{
    return dir_ + name + ".json";
}

} // namespace abcd
