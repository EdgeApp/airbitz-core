/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/util/Util.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::account, SettingsGet, "settings-get",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    AutoFree<tABC_AccountSettings, ABC_FreeAccountSettings> pSettings;
    ABC_CHECK_OLD(ABC_LoadAccountSettings(session.username.c_str(),
                                          session.password.c_str(),
                                          &pSettings.get(), &error));

    printf("First name: %s\n",
           pSettings->szFirstName ? pSettings->szFirstName : "(none)");
    printf("Last name: %s\n",
           pSettings->szLastName ? pSettings->szLastName : "(none)");
    printf("Nickname: %s\n",
           pSettings->szNickname ? pSettings->szNickname : "(none)");
    printf("PIN: %s\n", pSettings->szPIN ? pSettings->szPIN : "(none)");
    printf("List name on payments: %s\n",
           pSettings->bNameOnPayments ? "yes" : "no");
    printf("Seconds before auto logout: %d\n", pSettings->secondsAutoLogout);
    printf("Language: %s\n", pSettings->szLanguage);
    printf("Currency num: %d\n", pSettings->currencyNum);
    printf("Advanced features: %s\n", pSettings->bAdvancedFeatures ? "yes" : "no");
    std::cout << "Denomination satoshi: " << pSettings->bitcoinDenomination.satoshi
              << std::endl;
    printf("Denomination id: %d\n",
           pSettings->bitcoinDenomination.denominationType);
    printf("Daily Spend Enabled: %d\n", pSettings->bDailySpendLimit);
    printf("Daily Spend Limit: %ld\n", (long) pSettings->dailySpendLimitSatoshis);
    printf("PIN Spend Enabled: %d\n", pSettings->bSpendRequirePin);
    printf("PIN Spend Limit: %ld\n", (long) pSettings->spendRequirePinSatoshis);
    printf("Exchange rate source: %s\n", pSettings->szExchangeRateSource );

    return Status();
}

COMMAND(InitLevel::account, SettingsSetRecoveryReminder,
        "settings-set-recovery-reminder",
        " <n>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto count = atol(argv[0]);

    AutoFree<tABC_AccountSettings, ABC_FreeAccountSettings> pSettings;
    ABC_CHECK_OLD(ABC_LoadAccountSettings(session.username.c_str(),
                                          session.password.c_str(),
                                          &pSettings.get(), &error));

    std::cout << "Old reminder count: " << pSettings->recoveryReminderCount
              << std::endl;

    pSettings->recoveryReminderCount = count;
    ABC_CHECK_OLD(ABC_UpdateAccountSettings(session.username.c_str(),
                                            session.password.c_str(),
                                            pSettings, &error));

    return Status();
}

COMMAND(InitLevel::account, SettingsSetNickname, "settings-set-nickname",
        " <name>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto name = argv[0];

    AutoFree<tABC_AccountSettings, ABC_FreeAccountSettings> pSettings;
    ABC_CHECK_OLD(ABC_LoadAccountSettings(session.username.c_str(),
                                          session.password.c_str(),
                                          &pSettings.get(), &error));
    free(pSettings->szNickname);
    pSettings->szNickname = stringCopy(name);
    ABC_CHECK_OLD(ABC_UpdateAccountSettings(session.username.c_str(),
                                            session.password.c_str(),
                                            pSettings, &error));

    return Status();
}
