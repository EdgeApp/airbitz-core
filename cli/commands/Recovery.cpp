/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/login/LoginPassword.hpp"
#include "../../abcd/login/LoginRecovery.hpp"
#include "../../abcd/login/server/LoginServer.hpp"
#include "../../abcd/util/Util.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::store, RecoveryQuestions, "recovery-questions",
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

COMMAND(InitLevel::context, RecoveryQuestionChoices,
        "recovery-question-choices",
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

COMMAND(InitLevel::store, RecoveryLogin, "recovery-login",
        " <answers>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto answers = argv[0];

    AuthError authError;
    std::shared_ptr<Login> login;
    ABC_CHECK(loginRecovery(login, *session.store, answers, authError));

    return Status();
}

COMMAND(InitLevel::store, RecoveryChangePassword, "recovery-change-password",
        " <ra> <new-pass>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto answers = argv[0];
    const auto newPassword = argv[1];

    AuthError authError;
    std::shared_ptr<Login> login;
    ABC_CHECK(loginRecovery(login, *session.store, answers, authError));
    ABC_CHECK(loginPasswordSet(*login, newPassword));

    return Status();
}

COMMAND(InitLevel::login, RecoverySetup, "recovery-setup",
        " <questions> <answers>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto questions = argv[0];
    const auto answers = argv[1];

    ABC_CHECK(loginRecoverySet(*session.login, questions, answers));

    return Status();
}
