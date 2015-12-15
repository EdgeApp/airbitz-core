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

COMMAND(InitLevel::login, ChangePassword, "change-password")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... change-password <user> <pass> <new-pass>");
    const auto newPassword = argv[0];

    ABC_CHECK_OLD(ABC_ChangePassword(session.username.c_str(),
                                     session.password.c_str(),
                                     newPassword, &error));

    return Status();
}

COMMAND(InitLevel::lobby, ChangePasswordRecovery, "change-password-recovery")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... change-password-recovery <user> <ra> <new-pass>");
    const auto answers = argv[0];
    const auto newPassword = argv[1];

    ABC_CHECK_OLD(ABC_ChangePasswordWithRecoveryAnswers(session.username.c_str(),
                  answers, newPassword, &error));

    return Status();
}

COMMAND(InitLevel::context, CheckPassword, "check-password")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, "usage: ... check-password <pass>");
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

COMMAND(InitLevel::lobby, CheckRecoveryAnswers, "check-recovery-answers")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... check-recovery-answers <user> <ras>");
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

COMMAND(InitLevel::account, DataSync, "data-sync")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, "usage: ... data-sync <user> <pass>");

    ABC_CHECK(syncAll(*session.account));

    return Status();
}

COMMAND(InitLevel::context, GeneralUpdate, "general-update")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, "usage: ... general-update");

    ABC_CHECK(generalUpdate());

    return Status();
}

COMMAND(InitLevel::wallet, GenerateAddresses, "generate-addresses")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... generate-addresses <user> <pass> <wallet-name> <count>");
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

COMMAND(InitLevel::context, GetQuestionChoices, "get-question-choices")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, "usage: ... get-question-choices");

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

COMMAND(InitLevel::lobby, GetQuestions, "get-questions")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, "usage: ... get-questions <user>");

    AutoString questions;
    ABC_CHECK_OLD(ABC_GetRecoveryQuestions(session.username.c_str(),
                                           &questions.get(), &error));
    printf("Questions: %s\n", questions.get());

    return Status();
}

COMMAND(InitLevel::login, GetSettings, "get-settings")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, "usage: ... get-settings <user> <pass>");

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

COMMAND(InitLevel::context, ListAccounts, "list-accounts")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, "usage: ... list-accounts");

    AutoString usernames;
    ABC_CHECK_OLD(ABC_ListAccounts(&usernames.get(), &error));
    printf("Usernames:\n%s", usernames.get());

    return Status();
}

COMMAND(InitLevel::lobby, PinLogin, "pin-login")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, "usage: ... pin-login <user> <pin>");
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


COMMAND(InitLevel::account, PinLoginSetup, "pin-login-setup")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, "usage: ... pin-login-setup <user> <pass>");

    ABC_CHECK_OLD(ABC_PinSetup(session.username.c_str(),
                               session.password.c_str(), &error));

    return Status();
}

COMMAND(InitLevel::login, RecoveryReminderSet, "recovery-reminder-set")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... recovery-reminder-set <user> <pass> <n>");
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

COMMAND(InitLevel::wallet, SearchBitcoinSeed, "search-bitcoin-seed")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... search-bitcoin-seed <user> <pass> <wallet-name> <addr> <start> <end>");
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

COMMAND(InitLevel::account, SetNickname, "set-nickname")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, "usage: ... set-nickname <user> <pass> <name>");
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

COMMAND(InitLevel::lobby, SignIn, "sign-in")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, "usage: ... sign-in <user> <pass>");
    const auto password = argv[0];

    tABC_Error error;
    tABC_CC cc = ABC_SignIn(session.username.c_str(), password, &error);
    if (ABC_CC_InvalidOTP == cc)
    {
        AutoString date;
        ABC_CHECK_OLD(ABC_OtpResetDate(&date.get(), &error));
        if (strlen(date))
            std::cout << "Pending OTP reset ends at " << date.get() << std::endl;
        std::cout << "No OTP token, resetting account 2-factor auth." << std::endl;
        ABC_CHECK_OLD(ABC_OtpResetSet(session.username.c_str(), &error));
    }

    return Status();
}

COMMAND(InitLevel::account, UploadLogs, "upload-logs")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, "usage: ... upload-logs <user> <pass>");

    // TODO: Command non-functional without a watcher thread!
    ABC_CHECK_OLD(ABC_UploadLogs(session.username.c_str(),
                                 session.password.c_str(), &error));

    return Status();
}

COMMAND(InitLevel::none, Version, "version")
{
    AutoString version;
    ABC_CHECK_OLD(ABC_Version(&version.get(), &error));
    std::cout << "ABC version: " << version.get() << std::endl;
    return Status();
}
