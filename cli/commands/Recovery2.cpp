/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/crypto/Encoding.hpp"
#include "../../abcd/login/LoginStore.hpp"
#include "../../abcd/login/LoginPassword.hpp"
#include "../../abcd/login/LoginRecovery2.hpp"
#include "../../abcd/login/server/LoginServer.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::store, Recovery2Questions, "recovery2-questions",
        " <recovery2Key>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    DataChunk recovery2Key;
    ABC_CHECK(base58Decode(recovery2Key, argv[0]));

    std::list<std::string> questions;
    ABC_CHECK(loginRecovery2Questions(questions, *session.store, recovery2Key));

    std::cout << "Questions:" << std::endl;
    for (const auto i: questions)
        std::cout << "  " << i << std::endl;

    return Status();
}

COMMAND(InitLevel::store, Recovery2Key, "recovery2-key",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    AccountPaths paths;
    ABC_CHECK(session.store->paths(paths));

    DataChunk recovery2Key;
    ABC_CHECK(loginRecovery2Key(recovery2Key, paths));
    std::cout << "recovery2Key: " << base58Encode(recovery2Key) << std::endl;

    return Status();
}

COMMAND(InitLevel::store, Recovery2Login, "recovery2-login",
        " <recovery2Key> <answers>...")
{
    if (argc < 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    DataChunk recovery2Key;
    ABC_CHECK(base58Decode(recovery2Key, argv[0]));

    // Read answers:
    std::list<std::string> answers;
    for (int i = 1; i < argc; ++i)
        answers.push_back(argv[i]);

    // Log in:
    AuthError authError;
    std::shared_ptr<Login> login;
    ABC_CHECK(loginRecovery2(login, *session.store,
                             recovery2Key, answers, authError));

    return Status();
}

COMMAND(InitLevel::store, Recovery2ChangePassword, "recovery2-change-password",
        " <recovery2Key> <password> <answers>...")
{
    if (argc < 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    DataChunk recovery2Key;
    ABC_CHECK(base58Decode(recovery2Key, argv[0]));
    const auto password = argv[1];

    // Read answers:
    std::list<std::string> answers;
    for (int i = 2; i < argc; ++i)
        answers.push_back(argv[i]);

    // Log in:
    AuthError authError;
    std::shared_ptr<Login> login;
    ABC_CHECK(loginRecovery2(login, *session.store,
                             recovery2Key, answers, authError));
    ABC_CHECK(loginPasswordSet(*login, password));

    return Status();
}

COMMAND(InitLevel::login, Recovery2Setup, "recovery2-setup",
        " [<question> <answer>]...")
{
    if (argc % 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    // Gather the arguments:
    std::list<std::string> questions;
    std::list<std::string> answers;
    for (int i = 0; i < argc; i += 2)
    {
        questions.push_back(argv[i]);
        answers.push_back(argv[i + 1]);
    }

    DataChunk recovery2Key;
    ABC_CHECK(loginRecovery2Set(recovery2Key, *session.login, questions, answers));
    std::cout << "Please save the following key: " <<
              base58Encode(recovery2Key) << std::endl;

    return Status();
}
