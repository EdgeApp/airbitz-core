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
#include <dirent.h>

namespace abcd {

struct PluginNameJson:
    public JsonObject
{
    ABC_JSON_STRING(name, "name", nullptr)
};

constexpr auto nameFilename = "Name.json";

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
           cryptoFilename(account.dataKey(), plugin) + "/";
}

static std::string
keyFilename(const Account &account, const std::string &plugin,
            const std::string &key)
{
    return pluginDirectory(account, plugin) +
           cryptoFilename(account.dataKey(), key) + ".json";
}

std::list<std::string>
pluginDataList(const Account &account)
{
    std::list<std::string> out;

    std::string outer = pluginsDirectory(account);
    DIR *dir = opendir(outer.c_str());
    if (!dir)
        return out;

    struct dirent *de;
    while (nullptr != (de = readdir(dir)))
    {
        auto path = outer + de->d_name + '/' + nameFilename;
        PluginNameJson json;
        if (de->d_name[0] != '.'
                && json.load(path, account.dataKey())
                && json.nameOk())
            out.push_back(json.name());
    }

    closedir(dir);
    return out;
}

std::list<std::string>
pluginDataKeys(const Account &account, const std::string &plugin)
{
    std::list<std::string> out;

    std::string outer = pluginDirectory(account, plugin);
    DIR *dir = opendir(outer.c_str());
    if (!dir)
        return out;

    struct dirent *de;
    while (nullptr != (de = readdir(dir)))
    {
        PluginDataFile json;
        if (fileIsJson(de->d_name)
                && json.load(outer + de->d_name, account.dataKey())
                && json.keyOk())
            out.push_back(json.key());
    }

    closedir(dir);
    return out;
}

Status
pluginDataGet(const Account &account, const std::string &plugin,
              const std::string &key, std::string &data)
{
    PluginDataFile json;
    ABC_CHECK(json.load(keyFilename(account, plugin, key),
                        account.dataKey()));
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

    const auto namePath = pluginDirectory(account, plugin) + "Name.json";
    if (!fileExists(namePath))
    {
        PluginNameJson json;
        ABC_CHECK(json.nameSet(plugin));
        json.save(namePath, account.dataKey());
    }

    PluginDataFile json;
    json.keySet(key);
    json.dataSet(data);
    ABC_CHECK(json.save(keyFilename(account, plugin, key),
                        account.dataKey()));

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
