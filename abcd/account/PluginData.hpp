/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ACCOUNT_PLUGIN_DATA_HPP
#define ABCD_ACCOUNT_PLUGIN_DATA_HPP

#include "../util/Status.hpp"
#include <list>

namespace abcd {

class Account;

/**
 * Lists the keys in a plugin key/value store.
 */
std::list<std::string>
pluginDataKeys(const Account &account, const std::string &plugin);

/**
 * Retreives an item from the plugin key/value store.
 * @param plugin The plugin's unique ID.
 * @param key The data location. Merges happen at the key level,
 * so the account may contain a mix of keys from different devices.
 * The key contents are atomic, however. Place data accordingly.
 * @param data The value stored with the key.
 */
Status
pluginDataGet(const Account &account, const std::string &plugin,
              const std::string &key, std::string &data);

/**
 * Saves an item to the plugin key/value store.
 */
Status
pluginDataSet(const Account &account, const std::string &plugin,
              const std::string &key, const std::string &data);

/**
 * Deletes an item from the plugin key/value store.
 */
Status
pluginDataRemove(const Account &account, const std::string &plugin,
                 const std::string &key);

/**
 * Removes the entire key/value store for a particular plugin.
 */
Status
pluginDataClear(const Account &account, const std::string &plugin);

} // namespace abcd

#endif
