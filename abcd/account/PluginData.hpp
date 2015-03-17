/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ACCOUNT_PLUGIN_DATA_HPP
#define ABCD_ACCOUNT_PLUGIN_DATA_HPP

#include "../util/Status.hpp"

namespace abcd {

class Login;

/**
 * Retreives an item from the plugin key/value store.
 * @param plugin The plugin's unique ID.
 * @param key The data location. Merges happen at the key level,
 * so the account may contain a mix of keys from different devices.
 * The key contents are atomic, however. Place data accordingly.
 * @param data The value stored with the key.
 */
Status
pluginDataGet(const Login &login, const std::string &plugin,
    const std::string &key, std::string &data);

/**
 * Saves an item to the plugin key/value store.
 */
Status
pluginDataSet(const Login &login, const std::string &plugin,
    const std::string &key, const std::string &data);

/**
 * Deletes an item from the plugin key/value store.
 */
Status
pluginDataRemove(const Login &login, const std::string &plugin,
    const std::string &key);

/**
 * Removes the entire key/value store for a particular plugin.
 */
Status
pluginDataClear(const Login &login, const std::string &plugin);

} // namespace abcd

#endif
