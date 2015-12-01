/*
 * Copyright (c) 2015, AirBitz, Inc.
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

COMMAND(InitLevel::context, AccountAvailable, "account-available")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, "usage: ... account-available <user>");
    ABC_CHECK_OLD(ABC_AccountAvailable(argv[0], &error));
    return Status();
}

COMMAND(InitLevel::context, AccountCreate, "account-create")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, "usage: ... create-account <user> <pass>");

    ABC_CHECK_OLD(ABC_CreateAccount(argv[0], argv[1], &error));
    ABC_CHECK_OLD(ABC_SetPIN(argv[2], argv[3], "1234", &error));

    return Status();
}

COMMAND(InitLevel::account, AccountDecrypt, "account-decrypt")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... account-decrypt <user> <pass> <filename>\n"
                         "note: The filename is account-relative.");

    JsonBox box;
    ABC_CHECK(box.load(session.account->dir() + argv[2]));

    DataChunk data;
    ABC_CHECK(box.decrypt(data, session.login->dataKey()));
    std::cout << toString(data) << std::endl;

    return Status();
}

COMMAND(InitLevel::account, AccountEncrypt, "account-encrypt")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... account-encrypt <user> <pass> <filename>\n"
                         "note: The filename is account-relative.");

    DataChunk contents;
    ABC_CHECK(fileLoad(contents, session.account->dir() + argv[2]));

    JsonBox box;
    ABC_CHECK(box.encrypt(contents, session.login->dataKey()));

    std::cout << box.encode() << std::endl;

    return Status();
}
