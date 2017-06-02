/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ACCOUNT_ACCOUNT_SETTINGS_HPP
#define ABCD_ACCOUNT_ACCOUNT_SETTINGS_HPP

#include "../util/Status.hpp"

#define DEFAULT_SERVER_LIST "stratum://electrum.airbitz.co\nstratum://electrum-bu-az-weuro.airbitz.co:50001\nstratum://electrum-bu-az-wjapan.airbitz.co:50001\nstratum://electrum-bc-az-eusa.airbitz.co:50001"

// Miminum possible server string. ie. "stratum://e.c:1"
#define SERVER_STRING_LEN_MINIMUM 15

namespace abcd {

class Account;
class Login;

/**
 * Loads the settings from an account.
 * Returns default settings if anything goes wrong.
 */
tABC_AccountSettings *
accountSettingsLoad(Account &account);

/**
 * Saves the settings for an account.
 */
Status
accountSettingsSave(Account &account, tABC_AccountSettings *pSettings);

/**
 * Frees the account settings structure, along with its contents.
 */
void
accountSettingsFree(tABC_AccountSettings *pSettings);

} // namespace abcd

#endif
