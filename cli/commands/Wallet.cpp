/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../Util.hpp"
#include "../../abcd/account/Account.hpp"
#include "../../abcd/crypto/Encoding.hpp"
#include "../../abcd/exchange/Currency.hpp"
#include "../../abcd/json/JsonBox.hpp"
#include "../../abcd/util/FileIO.hpp"
#include "../../abcd/wallet/Wallet.hpp"
#include "../../src/LoginShim.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::wallet, CliWalletArchive, "wallet-archive")
{
    if (argc != 4)
        return ABC_ERROR(ABC_CC_Error, "usage: ... wallet-archive <user> <pass> <wallet-id> 1|0");

    ABC_CHECK(session.account->wallets.archivedSet(session.uuid, atoi(argv[3])));

    return Status();
}

COMMAND(InitLevel::account, CliWalletCreate, "wallet-create")
{
    if (argc != 4)
        return ABC_ERROR(ABC_CC_Error, "usage: ... wallet-create <user> <pass> <wallet-name> <currency>");

    Currency currency;
    ABC_CHECK(currencyNumber(currency, argv[3]));

    std::shared_ptr<Wallet> wallet;
    ABC_CHECK(cacheWalletNew(wallet, session.username, argv[2], static_cast<int>(currency)));
    std::cout << "Created wallet " << wallet->id() << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, CliWalletDecrypt, "wallet-decrypt")
{
    if (argc != 4)
        return ABC_ERROR(ABC_CC_Error, "usage: ... wallet-decrypt <user> <pass> <wallet-id> <file>");

    JsonBox box;
    ABC_CHECK(box.load(session.wallet->syncDir() + argv[3]));

    DataChunk data;
    ABC_CHECK(box.decrypt(data, session.wallet->dataKey()));
    std::cout << toString(data) << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, CliWalletEncrypt, "wallet-encrypt")
{
    if (argc != 4)
        return ABC_ERROR(ABC_CC_Error, "usage: ... wallet-encrypt <user> <pass> <wallet-id> <file>");

    DataChunk contents;
    ABC_CHECK(fileLoad(contents, session.wallet->syncDir() + argv[3]));

    JsonBox box;
    ABC_CHECK(box.encrypt(contents, session.wallet->dataKey()));

    std::cout << box.encode() << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, CliWalletInfo, "wallet-info")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... wallet-info <user> <pass> <wallet-id>");

    // Obtain the balance:
    WatcherThread thread;
    ABC_CHECK(thread.init(session));
    int64_t balance;
    ABC_CHECK(session.wallet->balance(balance));

    std::string currency;
    ABC_CHECK(currencyCode(currency, static_cast<Currency>(session.wallet->currency())));

    std::cout << "name:     " << session.wallet->name() << std::endl;
    std::cout << "currency: " << currency << std::endl;
    std::cout << "balance:  " << balance / 100000000.0 <<
        " (" << balance << " satoshis)" << std::endl;

    return Status();
}

COMMAND(InitLevel::account, CliWalletList, "wallet-list")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, "usage: ... wallet-list <user> <pass>");

    // Iterate over wallets:
    auto ids = session.account->wallets.list();
    for (const auto &id: ids)
    {
        std::shared_ptr<Wallet> wallet;
        ABC_CHECK(cacheWallet(wallet, nullptr, id.c_str()));
        std::cout << wallet->id() << ": " << wallet->name();

        bool archived;
        ABC_CHECK(session.account->wallets.archived(archived, id));
        if (archived)
            std::cout << " (archived)" << std::endl;
        else
            std::cout << std::endl;
    }

    return Status();
}

COMMAND(InitLevel::account, CliWalletOrder, "wallet-order")
{
    if (argc < 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... wallet-order <user> <pass> <wallet-ids>...");

    std::string ids;
    size_t count = argc - 2;
    for (size_t i = 0; i < count; ++i)
    {
        ids += argv[2 + i];
        ids += "\n";
    }

    ABC_CHECK_OLD(ABC_SetWalletOrder(session.username, session.password, ids.c_str(), &error));

    return Status();
}

COMMAND(InitLevel::wallet, CliWalletSeed, "wallet-seed")
{
    if (argc != 3)
        return ABC_ERROR(ABC_CC_Error, "usage: ... wallet-seed <user> <pass> <wallet-id>");

    std::cout << base16Encode(session.wallet->bitcoinKey()) << std::endl;

    return Status();
}
