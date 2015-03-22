/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "PluginData.hpp"
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
pluginsDirectory(const Login &login)
{
    return login.syncDir() + "Plugins/";
}

static std::string
pluginDirectory(const Login &login, const std::string &plugin)
{
    return pluginsDirectory(login) +
        cryptoFilename(login.dataKey(), plugin) + "/";
}

static std::string
keyFilename(const Login &login, const std::string &plugin,
    const std::string &key)
{
    return pluginDirectory(login, plugin) +
        cryptoFilename(login.dataKey(), key) + ".json";
}

Status
pluginDataGet(const Login &login, const std::string &plugin,
    const std::string &key, std::string &data)
{
    PluginDataFile json;
    ABC_CHECK(json.load(keyFilename(login, plugin, key), login.dataKey()));
    ABC_CHECK(json.keyOk());
    ABC_CHECK(json.dataOk());

    if (json.key() != key)
        return ABC_ERROR(ABC_CC_JSONError, "Plugin filename does not match contents");

    data = json.data();
    return Status();
}

Status
pluginDataSet(const Login &login, const std::string &plugin,
    const std::string &key, const std::string &data)
{
    ABC_CHECK(fileEnsureDir(pluginsDirectory(login)));
    ABC_CHECK(fileEnsureDir(pluginDirectory(login, plugin)));

    PluginDataFile json;
    json.keySet(key.c_str());
    json.dataSet(data.c_str());
    ABC_CHECK(json.save(keyFilename(login, plugin, key), login.dataKey()));

    return Status();
}

Status
pluginDataRemove(const Login &login, const std::string &plugin,
    const std::string &key)
{
    std::string filename = keyFilename(login, plugin, key);

    bool exists;
    ABC_CHECK_OLD(ABC_FileIOFileExists(filename.c_str(), &exists, &error));
    if (exists)
        ABC_CHECK_OLD(ABC_FileIODeleteFile(filename.c_str(), &error));

    return Status();
}

Status
pluginDataClear(const Login &login, const std::string &plugin)
{
    std::string directory = pluginDirectory(login, plugin);

    bool exists;
    ABC_CHECK_OLD(ABC_FileIOFileExists(directory.c_str(), &exists, &error));
    if (exists)
        ABC_CHECK_OLD(ABC_FileIODeleteRecursive(directory.c_str(), &error));

    return Status();
}

} // namespace abcd
