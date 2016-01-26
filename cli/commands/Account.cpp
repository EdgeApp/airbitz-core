/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/account/Account.hpp"
#include "../../abcd/json/JsonBox.hpp"
#include "../../abcd/login/Login.hpp"
#include "../../abcd/util/FileIO.hpp"
#include "../../abcd/util/Util.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::lobby, AccountAvailable, "account-available",
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
    ABC_CHECK_OLD(ABC_SetPIN(username, password, "1234", &error));

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
    ABC_CHECK(box.decrypt(data, session.login->dataKey()));
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
    ABC_CHECK(box.encrypt(contents, session.login->dataKey()));

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
