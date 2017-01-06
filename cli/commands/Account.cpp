/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/account/Account.hpp"
#include "../../abcd/crypto/Encoding.hpp"
#include "../../abcd/json/JsonBox.hpp"
#include "../../abcd/login/Login.hpp"
#include "../../abcd/util/FileIO.hpp"
#include "../../abcd/util/Util.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::store, AccountAvailable, "account-available",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    ABC_CHECK_OLD(ABC_AccountAvailable(session.username.c_str(), &error));
    return Status();
}

COMMAND(InitLevel::context, AccountCreate, "account-create",
        " <user> <pass>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto username = argv[0];
    const auto password = argv[1];

    ABC_CHECK_OLD(ABC_CreateAccount(username, password, &error));

    return Status();
}

COMMAND(InitLevel::login, AccountKey, "account-key",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    std::cout << base16Encode(session.login->dataKey()) << std::endl;

    return Status();
}

COMMAND(InitLevel::account, AccountDecrypt, "account-decrypt",
        " <filename>\n"
        "note: The filename is account-relative.")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto filename = argv[0];

    JsonBox box;
    ABC_CHECK(box.load(session.account->dir() + filename));

    DataChunk data;
    ABC_CHECK(box.decrypt(data, session.account->dataKey()));
    std::cout << toString(data) << std::endl;

    return Status();
}

COMMAND(InitLevel::account, AccountEncrypt, "account-encrypt",
        " <filename>\n"
        "note: The filename is account-relative.")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto filename = argv[0];

    DataChunk contents;
    ABC_CHECK(fileLoad(contents, session.account->dir() + filename));

    JsonBox box;
    ABC_CHECK(box.encrypt(contents, session.account->dataKey()));

    std::cout << box.encode() << std::endl;

    return Status();
}

COMMAND(InitLevel::context, AccountList, "account-list",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    AutoString usernames;
    ABC_CHECK_OLD(ABC_ListAccounts(&usernames.get(), &error));
    printf("Usernames:\n%s", usernames.get());

    return Status();
}

COMMAND(InitLevel::account, CliAccountSync, "account-sync",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    bool dirty;
    ABC_CHECK(session.account->sync(dirty));

    if (dirty)
        std::cout << "Contents changed" << std::endl;
    else
        std::cout << "No changes" << std::endl;

    return Status();
}
