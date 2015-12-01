/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../Util.hpp"
#include "../../abcd/General.hpp"
#include "../../abcd/util/Util.hpp"
#include "../../abcd/wallet/Wallet.hpp"
#include <bitcoin/bitcoin.hpp>
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::login, ChangePassword, "change-password",
        " <new-pass>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto newPassword = argv[0];

    ABC_CHECK_OLD(ABC_ChangePassword(session.username.c_str(),
                                     session.password.c_str(),
                                     newPassword, &error));

    return Status();
}

COMMAND(InitLevel::lobby, ChangePasswordRecovery, "change-password-recovery",
        " <ra> <new-pass>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto answers = argv[0];
    const auto newPassword = argv[1];

    ABC_CHECK_OLD(ABC_ChangePasswordWithRecoveryAnswers(session.username.c_str(),
                  answers, newPassword, &error));

    return Status();
}

COMMAND(InitLevel::context, CheckPassword, "check-password",
        " <pass>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto password = argv[0];

    double secondsToCrack;
    unsigned int count = 0;
    tABC_PasswordRule **aRules = NULL;
    ABC_CHECK_OLD(ABC_CheckPassword(password, &secondsToCrack, &aRules, &count,
                                    &error));

    for (unsigned i = 0; i < count; ++i)
    {
        printf("%s: %d\n", aRules[i]->szDescription, aRules[i]->bPassed);
    }
    printf("Time to Crack: %f\n", secondsToCrack);
    ABC_FreePasswordRuleArray(aRules, count);

    return Status();
}

COMMAND(InitLevel::lobby, CheckRecoveryAnswers, "check-recovery-answers",
        " <answers>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto answers = argv[0];

    AutoString szQuestions;
    ABC_CHECK_OLD(ABC_GetRecoveryQuestions(session.username.c_str(),
                                           &szQuestions.get(), &error));
    printf("%s\n", szQuestions.get());

    bool bValid = false;
    ABC_CHECK_OLD(ABC_CheckRecoveryAnswers(session.username.c_str(),
                                           answers, &bValid, &error));
    printf("%s\n", bValid ? "Valid!" : "Invalid!");

    return Status();
}

COMMAND(InitLevel::account, DataSync, "data-sync",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    ABC_CHECK(syncAll(*session.account));

    return Status();
}

COMMAND(InitLevel::context, GeneralUpdate, "general-update",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    ABC_CHECK(generalUpdate());

    return Status();
}

COMMAND(InitLevel::wallet, GenerateAddresses, "generate-addresses",
        " <count>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto count = atol(argv[0]);

    bc::hd_private_key m(session.wallet->bitcoinKey());
    bc::hd_private_key m0 = m.generate_private_key(0);
    bc::hd_private_key m00 = m0.generate_private_key(0);
    for (int i = 0; i < count; ++i)
    {
        bc::hd_private_key m00n = m00.generate_private_key(i);
        std::cout << "watch " << m00n.address().encoded() << std::endl;
    }

    return Status();
}

COMMAND(InitLevel::context, GetQuestionChoices, "get-question-choices",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    AutoFree<tABC_QuestionChoices, ABC_FreeQuestionChoices> pChoices;
    ABC_CHECK_OLD(ABC_GetQuestionChoices(&pChoices.get(), &error));

    printf("Choices:\n");
    for (unsigned i = 0; i < pChoices->numChoices; ++i)
    {
        printf(" %s (%s, %d)\n", pChoices->aChoices[i]->szQuestion,
               pChoices->aChoices[i]->szCategory,
               pChoices->aChoices[i]->minAnswerLength);
    }

    return Status();
}

COMMAND(InitLevel::lobby, GetQuestions, "get-questions",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    AutoString questions;
    ABC_CHECK_OLD(ABC_GetRecoveryQuestions(session.username.c_str(),
                                           &questions.get(), &error));
    printf("Questions: %s\n", questions.get());

    return Status();
}

COMMAND(InitLevel::login, GetSettings, "get-settings",
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
    printf("Minutes before auto logout: %d\n", pSettings->minutesAutoLogout);
    printf("Language: %s\n", pSettings->szLanguage);
    printf("Currency num: %d\n", pSettings->currencyNum);
    printf("Advanced features: %s\n", pSettings->bAdvancedFeatures ? "yes" : "no");
    printf("Denomination satoshi: %ld\n", pSettings->bitcoinDenomination.satoshi);
    printf("Denomination id: %d\n",
           pSettings->bitcoinDenomination.denominationType);
    printf("Daily Spend Enabled: %d\n", pSettings->bDailySpendLimit);
    printf("Daily Spend Limit: %ld\n", (long) pSettings->dailySpendLimitSatoshis);
    printf("PIN Spend Enabled: %d\n", pSettings->bSpendRequirePin);
    printf("PIN Spend Limit: %ld\n", (long) pSettings->spendRequirePinSatoshis);
    printf("Exchange rate source: %s\n", pSettings->szExchangeRateSource );

    return Status();
}

COMMAND(InitLevel::context, ListAccounts, "list-accounts",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    AutoString usernames;
    ABC_CHECK_OLD(ABC_ListAccounts(&usernames.get(), &error));
    printf("Usernames:\n%s", usernames.get());

    return Status();
}

COMMAND(InitLevel::lobby, PinLogin, "pin-login",
        " pin>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto pin = argv[0];

    bool bExists;
    ABC_CHECK_OLD(ABC_PinLoginExists(session.username.c_str(),
                                     &bExists, &error));
    if (bExists)
    {
        ABC_CHECK_OLD(ABC_PinLogin(session.username.c_str(), pin, &error));
    }
    else
    {
        printf("Login expired\n");
    }

    return Status();
}


COMMAND(InitLevel::account, PinLoginSetup, "pin-login-setup",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    ABC_CHECK_OLD(ABC_PinSetup(session.username.c_str(),
                               session.password.c_str(), &error));

    return Status();
}

COMMAND(InitLevel::login, RecoveryReminderSet, "recovery-reminder-set",
        " <n>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto count = atol(argv[0]);

    AutoFree<tABC_AccountSettings, ABC_FreeAccountSettings> pSettings;
    ABC_CHECK_OLD(ABC_LoadAccountSettings(session.username.c_str(),
                                          session.password.c_str(),
                                          &pSettings.get(), &error));
    printf("Old Reminder Count: %d\n", pSettings->recoveryReminderCount);

    pSettings->recoveryReminderCount = count;
    ABC_CHECK_OLD(ABC_UpdateAccountSettings(session.username.c_str(),
                                            session.password.c_str(),
                                            pSettings, &error));

    return Status();
}

COMMAND(InitLevel::wallet, SearchBitcoinSeed, "search-bitcoin-seed",
        " <addr> <start> <end>")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto address = argv[0];
    const auto start = atol(argv[1]);
    const auto end = atol(argv[2]);

    bc::hd_private_key m(session.wallet->bitcoinKey());
    bc::hd_private_key m0 = m.generate_private_key(0);
    bc::hd_private_key m00 = m0.generate_private_key(0);

    for (long i = start, c = 0; i <= end; i++, ++c)
    {
        bc::hd_private_key m00n = m00.generate_private_key(i);
        if (m00n.address().encoded() == address)
        {
            printf("Found %s at %ld\n", address, i);
            break;
        }
        if (c == 100000)
        {
            printf("%ld\n", i);
            c = 0;
        }
    }

    return Status();
}

COMMAND(InitLevel::account, SetNickname, "set-nickname",
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

COMMAND(InitLevel::login, SignIn, "sign-in",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    return Status();
}

COMMAND(InitLevel::account, UploadLogs, "upload-logs",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    // TODO: Command non-functional without a watcher thread!
    ABC_CHECK_OLD(ABC_UploadLogs(session.username.c_str(),
                                 session.password.c_str(), &error));

    return Status();
}

COMMAND(InitLevel::none, Version, "version",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    AutoString version;
    ABC_CHECK_OLD(ABC_Version(&version.get(), &error));
    std::cout << "ABC version: " << version.get() << std::endl;
    return Status();
}
