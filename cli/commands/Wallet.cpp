/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/account/Account.hpp"
#include "../../abcd/crypto/Encoding.hpp"
#include "../../abcd/exchange/Currency.hpp"
#include "../../abcd/json/JsonBox.hpp"
#include "../../abcd/util/FileIO.hpp"
#include "../../abcd/wallet/Wallet.hpp"
#include "../../src/LoginShim.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::wallet, CliWalletArchive, "wallet-archive",
        " 0|1")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto archive = atoi(argv[0]);

    ABC_CHECK(session.account->wallets.archivedSet(session.uuid, archive));

    return Status();
}

COMMAND(InitLevel::account, CliWalletCreate, "wallet-create",
        " <name> <currency>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto name = argv[0];
    const auto currencyName = argv[1];

    Currency currency;
    ABC_CHECK(currencyNumber(currency, currencyName));

    std::shared_ptr<Wallet> wallet;
    ABC_CHECK(cacheWalletNew(wallet, session.username.c_str(), name,
                             static_cast<int>(currency)));
    std::cout << "Created wallet " << wallet->id() << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, CliWalletDecrypt, "wallet-decrypt",
        " <filename>\n"
        "note: The filename is relative to the wallet sync directory.")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto filename = argv[0];

    JsonBox box;
    ABC_CHECK(box.load(session.wallet->paths.syncDir() + filename));

    DataChunk data;
    ABC_CHECK(box.decrypt(data, session.wallet->dataKey()));
    std::cout << toString(data) << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, CliWalletEncrypt, "wallet-encrypt",
        " <filename>\n"
        "note: The filename is relative to the wallet sync directory.")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto filename = argv[0];

    DataChunk contents;
    ABC_CHECK(fileLoad(contents, session.wallet->paths.syncDir() + filename));

    JsonBox box;
    ABC_CHECK(box.encrypt(contents, session.wallet->dataKey()));

    std::cout << box.encode() << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, CliWalletInfo, "wallet-info",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    // Obtain the balance:
    int64_t balance;
    ABC_CHECK(session.wallet->balance(balance));

    std::string currency;
    ABC_CHECK(currencyCode(currency,
                           static_cast<Currency>(session.wallet->currency())));

    std::cout << "name:     " << session.wallet->name() << std::endl;
    std::cout << "currency: " << currency << std::endl;
    std::cout << "balance:  " << balance / 100000000.0 <<
              " (" << balance << " satoshis)" << std::endl;

    return Status();
}

COMMAND(InitLevel::account, CliWalletList, "wallet-list",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    // Load wallets:
    auto ids = session.account->wallets.list();
    std::list<std::shared_ptr<Wallet>> wallets;
    for (const auto &id: ids)
    {
        std::shared_ptr<Wallet> wallet;
        ABC_CHECK(cacheWallet(wallet, nullptr, id.c_str()));
        wallets.push_back(wallet);
    }

    // Display wallets:
    for (const auto &wallet: wallets)
    {
        std::cout << wallet->id() << ": " << wallet->name();

        bool archived;
        ABC_CHECK(session.account->wallets.archived(archived, wallet->id()));
        if (archived)
            std::cout << " (archived)" << std::endl;
        else
            std::cout << std::endl;
    }

    return Status();
}

COMMAND(InitLevel::account, CliWalletOrder, "wallet-order",
        " <wallet-ids>...")
{
    if (argc < 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    std::string ids;
    size_t count = argc;
    for (size_t i = 0; i < count; ++i)
    {
        ids += argv[i];
        ids += "\n";
    }

    ABC_CHECK_OLD(ABC_SetWalletOrder(session.username.c_str(),
                                     session.password.c_str(),
                                     ids.c_str(), &error));

    return Status();
}

COMMAND(InitLevel::wallet, CliWalletSeed, "wallet-seed",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    std::cout << base16Encode(session.wallet->bitcoinKey()) << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, CliWalletRemove, "wallet-remove",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    ABC_CHECK(cacheWalletRemove(session.username.c_str(), session.uuid.c_str()));
    std::cout << "Removed wallet " << session.wallet->name() << std::endl;
    return Status();
}
