/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "PluginData.hpp"
#include "../crypto/Crypto.hpp"
#include "../json/JsonObject.hpp"
#include "../util/FileIO.hpp"
#include "../util/Sync.hpp"
#include <sstream>

namespace abcd {

struct PluginDataFile:
    public JsonObject
{
    PluginDataFile() {}
    PluginDataFile(json_t *root): JsonObject(root) {}

    ABC_JSON_STRING(Key,  "key",  nullptr)
    ABC_JSON_STRING(Data, "data", nullptr)
};

static std::string
pluginsDirectory(tABC_SyncKeys *pKeys)
{
    return std::string(pKeys->szSyncDir) + "/Plugins/";
}

static std::string
pluginDirectory(tABC_SyncKeys *pKeys, const std::string &plugin)
{
    return pluginsDirectory(pKeys) +
        cryptoFilename(pKeys->MK, plugin) + "/";
}

static std::string
keyFilename(tABC_SyncKeys *pKeys, const std::string &plugin,
    const std::string &key)
{
    return pluginDirectory(pKeys, plugin) +
        cryptoFilename(pKeys->MK, key) + ".json";
}

Status
pluginDataGet(tABC_SyncKeys *pKeys, const std::string &plugin,
    const std::string &key, std::string &data)
{
    std::string filename = keyFilename(pKeys, plugin, key);

    json_t *temp;
    ABC_CHECK_OLD(ABC_CryptoDecryptJSONFileObject(filename.c_str(), pKeys->MK,
        &temp, &error));
    PluginDataFile json(temp);
    ABC_CHECK(json.hasKey());
    ABC_CHECK(json.hasData());
    data = json.getData();

    return Status();
}

Status
pluginDataSet(tABC_SyncKeys *pKeys, const std::string &plugin,
    const std::string &key, const std::string &data)
{
    ABC_CHECK(fileEnsureDir(pluginsDirectory(pKeys)));
    ABC_CHECK(fileEnsureDir(pluginDirectory(pKeys, plugin)));

    PluginDataFile json;
    json.setKey(key.c_str());
    json.setData(data.c_str());
    ABC_CHECK_OLD(ABC_CryptoEncryptJSONFileObject(json.root(), pKeys->MK,
        ABC_CryptoType_AES256, keyFilename(pKeys, plugin, key).c_str(), &error));

    return Status();
}

Status
pluginDataRemove(tABC_SyncKeys *pKeys, const std::string &plugin,
    const std::string &key)
{
    std::string filename = keyFilename(pKeys, plugin, key);

    bool exists;
    ABC_CHECK_OLD(ABC_FileIOFileExists(filename.c_str(), &exists, &error));
    if (exists)
        ABC_CHECK_OLD(ABC_FileIODeleteFile(filename.c_str(), &error));

    return Status();
}

Status
pluginDataClear(tABC_SyncKeys *pKeys, const std::string &plugin)
{
    std::string directory = pluginDirectory(pKeys, plugin);

    bool exists;
    ABC_CHECK_OLD(ABC_FileIOFileExists(directory.c_str(), &exists, &error));
    if (exists)
        ABC_CHECK_OLD(ABC_FileIODeleteRecursive(directory.c_str(), &error));

    return Status();
}

} // namespace abcd
