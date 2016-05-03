/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "AddressDb.hpp"
#include "Wallet.hpp"
#include "../bitcoin/TxCache.hpp"
#include "../crypto/Crypto.hpp"
#include "../json/JsonObject.hpp"
#include "../util/Debug.hpp"
#include "../util/FileIO.hpp"
#include <bitcoin/bitcoin.hpp>
#include <dirent.h>
#include <time.h>

namespace abcd {

struct AddressStateJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(AddressStateJson, JsonObject)

    ABC_JSON_BOOLEAN(recyclable, "recycleable", true)
    ABC_JSON_INTEGER(time, "creationDate", 0)
};

struct AddressJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(AddressJson, JsonObject)

    ABC_JSON_INTEGER(index, "seq", 0)
    ABC_JSON_STRING(address, "address", nullptr)
    ABC_JSON_VALUE(state, "state", AddressStateJson)
    ABC_JSON_VALUE(metadata, "meta", JsonPtr)

    Status
    pack(const AddressMeta &in);

    Status
    unpack(AddressMeta &result);
};

Status
AddressJson::pack(const AddressMeta &in)
{
    // Main json:
    ABC_CHECK(indexSet(in.index));
    ABC_CHECK(addressSet(in.address));

    // State json:
    AddressStateJson stateJson;
    ABC_CHECK(stateJson.recyclableSet(in.recyclable));
    ABC_CHECK(stateJson.timeSet(in.time));
    ABC_CHECK(stateSet(stateJson));

    // Details json:
    JsonPtr metaJson;
    ABC_CHECK(in.metadata.save(metaJson));
    ABC_CHECK(metadataSet(metaJson));

    return Status();
}

Status
AddressJson::unpack(AddressMeta &result)
{
    AddressMeta out;

    // Main json:
    ABC_CHECK(indexOk());
    out.index = index();
    ABC_CHECK(addressOk());
    out.address = address();

    // State json:
    AddressStateJson stateJson = state();
    out.recyclable = stateJson.recyclable();
    out.time = stateJson.time();

    // Details json:
    ABC_CHECK(out.metadata.load(metadata()));

    result = std::move(out);
    return Status();
}

static bc::hd_private_key
mainBranch(const Wallet &wallet)
{
    return bc::hd_private_key(wallet.bitcoinKey()).
           generate_private_key(0).
           generate_private_key(0);
}

AddressDb::AddressDb(Wallet &wallet):
    wallet_(wallet),
    dir_(wallet.paths.addressesDir())
{
}

Status
AddressDb::load()
{
    std::lock_guard<std::mutex> lock(mutex_);

    addresses_.clear();
    files_.clear();

    // Open the directory:
    DIR *dir = opendir(dir_.c_str());
    if (dir)
    {
        struct dirent *de;
        while (nullptr != (de = readdir(dir)))
        {
            if (!fileIsJson(de->d_name))
                continue;

            // Try to load the address:
            AddressMeta address;
            AddressJson json;
            if (json.load(dir_ + de->d_name, wallet_.dataKey()).log() &&
                    json.unpack(address).log())
            {
                if (path(address) != dir_ + de->d_name)
                    ABC_DebugLog("Filename %s does not match address", de->d_name);

                addresses_[address.address] = address;
                files_[address.address] = json;

                wallet_.addressCache.insert(address.address);
            }
        }
        closedir(dir);
    }

    ABC_CHECK(stockpile());
    return Status();
}

Status
AddressDb::save(const AddressMeta &address)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = addresses_.find(address.address);
    if (i == addresses_.end())
        return ABC_ERROR(ABC_CC_NoAvailableAddress, "No address: " + address.address);
    addresses_[address.address] = address;

    AddressJson json(files_[address.address]);
    if (!json)
        json = JsonObject();
    ABC_CHECK(json.pack(address));
    ABC_CHECK(json.save(path(address), wallet_.dataKey()));
    files_[address.address] = json;

    ABC_CHECK(stockpile());
    return Status();
}

AddressSet
AddressDb::list() const
{
    std::lock_guard<std::mutex> lock(mutex_);

    AddressSet out;
    for (const auto &i: addresses_)
        out.insert(i.first);

    return out;
}

KeyTable
AddressDb::keyTable()
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto m00 = mainBranch(wallet_);
    KeyTable out;
    for (const auto &i: addresses_)
    {
        out[i.first] = bc::secret_to_wif(
                           m00.generate_private_key(i.second.index).private_key());
    }

    return out;
}

bool
AddressDb::has(const std::string &address)
{
    std::lock_guard<std::mutex> lock(mutex_);

    return addresses_.end() != addresses_.find(address);
}

Status
AddressDb::get(AddressMeta &result, const std::string &address)
{
    std::lock_guard<std::mutex> lock(mutex_);

    auto i = addresses_.find(address);
    if (i == addresses_.end())
        return ABC_ERROR(ABC_CC_NoAvailableAddress, "No address: " + address);

    result = i->second;
    return Status();
}

Status
AddressDb::getNew(AddressMeta &result)
{
    std::lock_guard<std::mutex> lock(mutex_);

    // Use a set to find the lowest available index:
    std::set<size_t> indices;
    for (auto &address: addresses_)
        if (address.second.recyclable)
            indices.insert(address.second.index);

    // The stockpile should prevent this from ever happening:
    if (indices.empty())
        return ABC_ERROR(ABC_CC_NoAvailableAddress, "Address stockpile depleted!");
    size_t index = *indices.begin();

    // Verify that we can still re-derive the address:
    auto i = addresses_.find(mainBranch(wallet_).generate_private_key(index).
                             address().encoded());
    if (addresses_.end() == i)
        return ABC_ERROR(ABC_CC_Error,
                         "Address corruption at index " + std::to_string(index));

    result = i->second;
    return Status();
}

Status
AddressDb::recycleSet(const std::string &address, bool recycle)
{
    AddressMeta a;
    ABC_CHECK(get(a, address));
    if (a.recyclable != recycle)
    {
        a.recyclable = recycle;
        ABC_CHECK(save(a));
    }

    return Status();
}

/**
 * Marks a transaction's output addresses as having received money.
 */
Status
AddressDb::markOutputs(const std::list<TxInOut> &ios)
{
    for (const auto &io: ios)
    {
        if (!io.input)
            recycleSet(io.address, false); /* Failure is ok. */
    }

    return Status();
}

Status
AddressDb::stockpile()
{
    ABC_CHECK(fileEnsureDir(dir_));

    // Build a list of used indices:
    std::map<size_t, bool> indices;
    for (const auto &i: addresses_)
        indices[i.second.index] = i.second.recyclable;

    // Check for gaps:
    size_t lastUsed = 0;
    for (size_t i = 0; i < addresses_.size() || i < lastUsed + 5; ++i)
    {
        auto index = indices.find(i);
        if (index == indices.end())
        {
            // Create the missing address:
            auto m00n = mainBranch(wallet_).generate_private_key(i);
            if (m00n.valid())
            {
                AddressMeta address;
                address.index = i;
                address.address = m00n.address().encoded();
                address.recyclable = true;
                address.time = time(nullptr);
                addresses_[address.address] = address;

                AddressJson json;
                ABC_CHECK(json.pack(address));
                ABC_CHECK(json.save(path(address), wallet_.dataKey()));
                files_[address.address] = json;

                wallet_.addressCache.insert(address.address);
            }
        }
        else if (!index->second)
        {
            lastUsed = i;
        }
    }

    return Status();
}

std::string
AddressDb::path(const AddressMeta &address)
{
    return dir_ + std::to_string(address.index) + "-" +
           cryptoFilename(wallet_.dataKey(), address.address) + ".json";
}

} // namespace abcd
