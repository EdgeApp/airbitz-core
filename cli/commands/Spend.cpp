/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../Util.hpp"
#include "../../abcd/util/Util.hpp"
#include <iostream>

using namespace abcd;

static void
syncCallback(const tABC_AsyncBitCoinInfo *pInfo)
{
}

COMMAND(InitLevel::wallet, SpendUri, "spend-uri",
        " <uri>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto uri = argv[0];

    WatcherThread thread;
    ABC_CHECK(thread.init(session));

    AutoFree<tABC_SpendTarget, ABC_SpendTargetFree> pSpend;
    ABC_CHECK_OLD(ABC_SpendNewDecode(uri, &pSpend.get(), &error));
    std::cout << "Sending " << pSpend->amount << " satoshis to " << pSpend->szName
              << std::endl;

    AutoString szTxId;
    ABC_CHECK_OLD(ABC_SpendApprove(session.username.c_str(),
                                   session.uuid.c_str(),
                                   pSpend, &szTxId.get(), &error));
    std::cout << "Transaction id: " << szTxId.get() << std::endl;

    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username.c_str(),
                                     session.password.c_str(),
                                     session.uuid.c_str(),
                                     syncCallback, nullptr, &error));

    return Status();
}

COMMAND(InitLevel::wallet, SpendTransfer, "spend-transfer",
        " <wallet-dest> <amount>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto dest = argv[0];
    const auto amount = atol(argv[1]);

    Session sessionDest = session;
    sessionDest.uuid = dest;
    WatcherThread threadDest;
    ABC_CHECK(threadDest.init(sessionDest));

    WatcherThread thread;
    ABC_CHECK(thread.init(session));

    AutoFree<tABC_SpendTarget, ABC_SpendTargetFree> pSpend;
    ABC_CHECK_OLD(ABC_SpendNewTransfer(session.username.c_str(),
                                       dest, amount, &pSpend.get(), &error));
    std::cout << "Sending " << pSpend->amount << " satoshis to " << pSpend->szName
              << std::endl;

    AutoString szTxId;
    ABC_CHECK_OLD(ABC_SpendApprove(session.username.c_str(),
                                   session.uuid.c_str(),
                                   pSpend, &szTxId.get(), &error));
    std::cout << "Transaction id: " << szTxId.get() << std::endl;

    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username.c_str(),
                                     session.password.c_str(),
                                     session.uuid.c_str(),
                                     syncCallback, nullptr, &error));
    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username.c_str(),
                                     session.password.c_str(),
                                     dest, syncCallback, nullptr, &error));

    return Status();
}

COMMAND(InitLevel::wallet, SpendInternal, "spend-internal",
        " <address> <amount>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto address = argv[0];
    const auto amount = atol(argv[1]);

    WatcherThread thread;
    ABC_CHECK(thread.init(session));

    AutoFree<tABC_SpendTarget, ABC_SpendTargetFree> pSpend;
    ABC_CHECK_OLD(ABC_SpendNewInternal(address, nullptr, nullptr, nullptr, amount,
                                       &pSpend.get(), &error));
    std::cout << "Sending " << pSpend->amount << " satoshis to " << pSpend->szName
              << std::endl;

    AutoString szTxId;
    ABC_CHECK_OLD(ABC_SpendApprove(session.username.c_str(),
                                   session.uuid.c_str(),
                                   pSpend, &szTxId.get(), &error));
    std::cout << "Transaction id: " << szTxId.get() << std::endl;

    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username.c_str(),
                                     session.password.c_str(),
                                     session.uuid.c_str(),
                                     syncCallback, nullptr, &error));

    return Status();
}
