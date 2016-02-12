/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ACCOUNT_ACCOUNT_SETTINGS_HPP
#define ABCD_ACCOUNT_ACCOUNT_SETTINGS_HPP

#include "../util/Status.hpp"

namespace abcd {

class Account;
class Login;

/**
 * Loads the settings from an account.
 * Returns default settings if anything goes wrong.
 */
tABC_AccountSettings *
accountSettingsLoad(const Account &account);

/**
 * Saves the settings for an account.
 */
Status
accountSettingsSave(const Account &account, tABC_AccountSettings *pSettings,
                    bool pinChanged=false);

/**
 * Frees the account settings structure, along with its contents.
 */
void
accountSettingsFree(tABC_AccountSettings *pSettings);

/**
 * Updates the local PIN package to match the settings.
 */
Status
accountSettingsPinSync(Login &login, tABC_AccountSettings *settings,
                       bool pinChanged);

} // namespace abcd

#endif
