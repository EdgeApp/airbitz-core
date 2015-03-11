/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_ACCOUNT_ACCOUNT_SETTINGS_HPP
#define ABCD_ACCOUNT_ACCOUNT_SETTINGS_HPP

#include "../util/Sync.hpp"

namespace abcd {

tABC_CC ABC_AccountSettingsLoad(tABC_SyncKeys *pKeys,
                                tABC_AccountSettings **ppSettings,
                                tABC_Error *pError);

tABC_CC ABC_AccountSettingsSave(tABC_SyncKeys *pKeys,
                                tABC_AccountSettings *pSettings,
                                tABC_Error *pError);

void ABC_AccountSettingsFree(tABC_AccountSettings *pSettings);

} // namespace abcd

#endif
