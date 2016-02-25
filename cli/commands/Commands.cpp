/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../Util.hpp"
#include "../../abcd/General.hpp"
#include "../../abcd/util/Util.hpp"
#include "../../abcd/wallet/Wallet.hpp"
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

    ABC_CHECK_OLD(ABC_RecoveryLogin(session.username.c_str(),
                                    answers, &error));
    ABC_CHECK_OLD(ABC_ChangePassword(session.username.c_str(), nullptr,
                                     newPassword, &error));

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

    ABC_CHECK_OLD(ABC_RecoveryLogin(session.username.c_str(),
                                    answers, &error));

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
        " <pin>")
{
    if (1 != argc)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto pin = argv[0];

    ABC_CHECK_OLD(ABC_PinSetup(session.username.c_str(),
                               session.password.c_str(),
                               pin, &error));

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
