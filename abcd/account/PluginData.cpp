/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "PluginData.hpp"
#include "Account.hpp"
#include "../crypto/Crypto.hpp"
#include "../json/JsonObject.hpp"
#include "../login/Login.hpp"
#include "../util/FileIO.hpp"
#include <sstream>

namespace abcd {

struct PluginDataFile:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(PluginDataFile, JsonObject)

    ABC_JSON_STRING(key,  "key",  nullptr)
    ABC_JSON_STRING(data, "data", nullptr)
};

static std::string
pluginsDirectory(const Account &account)
{
    return account.dir() + "Plugins/";
}

static std::string
pluginDirectory(const Account &account, const std::string &plugin)
{
    return pluginsDirectory(account) +
           cryptoFilename(account.login.dataKey(), plugin) + "/";
}

static std::string
keyFilename(const Account &account, const std::string &plugin,
            const std::string &key)
{
    return pluginDirectory(account, plugin) +
           cryptoFilename(account.login.dataKey(), key) + ".json";
}

Status
pluginDataGet(const Account &account, const std::string &plugin,
              const std::string &key, std::string &data)
{
    PluginDataFile json;
    ABC_CHECK(json.load(keyFilename(account, plugin, key),
                        account.login.dataKey()));
    ABC_CHECK(json.keyOk());
    ABC_CHECK(json.dataOk());

    if (json.key() != key)
        return ABC_ERROR(ABC_CC_JSONError, "Plugin filename does not match contents");

    data = json.data();
    return Status();
}

Status
pluginDataSet(const Account &account, const std::string &plugin,
              const std::string &key, const std::string &data)
{
    ABC_CHECK(fileEnsureDir(pluginsDirectory(account)));
    ABC_CHECK(fileEnsureDir(pluginDirectory(account, plugin)));

    PluginDataFile json;
    json.keySet(key);
    json.dataSet(data);
    ABC_CHECK(json.save(keyFilename(account, plugin, key),
                        account.login.dataKey()));

    return Status();
}

Status
pluginDataRemove(const Account &account, const std::string &plugin,
                 const std::string &key)
{
    std::string filename = keyFilename(account, plugin, key);

    if (fileExists(filename))
        ABC_CHECK(fileDelete(filename));

    return Status();
}

Status
pluginDataClear(const Account &account, const std::string &plugin)
{
    std::string directory = pluginDirectory(account, plugin);

    if (fileExists(directory))
        ABC_CHECK(fileDelete(directory));

    return Status();
}

} // namespace abcd
