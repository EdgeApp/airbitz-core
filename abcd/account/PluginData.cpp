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

    ABC_JSON_STRING(Key,  "key",  nullptr)
    ABC_JSON_STRING(Data, "data", nullptr)
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
    std::string filename = keyFilename(login, plugin, key);

    json_t *temp;
    ABC_CHECK_OLD(ABC_CryptoDecryptJSONFileObject(filename.c_str(),
        toU08Buf(login.dataKey()), &temp, &error));
    PluginDataFile json(temp);
    ABC_CHECK(json.hasKey());
    ABC_CHECK(json.hasData());
    data = json.getData();

    return Status();
}

Status
pluginDataSet(const Login &login, const std::string &plugin,
    const std::string &key, const std::string &data)
{
    ABC_CHECK(fileEnsureDir(pluginsDirectory(login)));
    ABC_CHECK(fileEnsureDir(pluginDirectory(login, plugin)));

    PluginDataFile json;
    json.setKey(key.c_str());
    json.setData(data.c_str());
    ABC_CHECK_OLD(ABC_CryptoEncryptJSONFileObject(json.get(),
        toU08Buf(login.dataKey()), ABC_CryptoType_AES256,
        keyFilename(login, plugin, key).c_str(), &error));

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
