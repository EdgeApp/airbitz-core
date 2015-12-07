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

COMMAND(InitLevel::wallet, SpendUri, "spend-uri")
{
    if (argc != 4)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... spend-uri <user> <pass> <wallet> <uri>");
    const char *uri = argv[3];

    WatcherThread thread;
    ABC_CHECK(thread.init(session));

    AutoFree<tABC_SpendTarget, ABC_SpendTargetFree> pSpend;
    ABC_CHECK_OLD(ABC_SpendNewDecode(uri, &pSpend.get(), &error));
    std::cout << "Sending " << pSpend->amount << " satoshis to " << pSpend->szName
              << std::endl;

    AutoString szTxId;
    ABC_CHECK_OLD(ABC_SpendApprove(session.username, session.uuid, pSpend,
                                   &szTxId.get(), &error));
    std::cout << "Transaction id: " << szTxId.get() << std::endl;

    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username, session.password,
                                     session.uuid, syncCallback, nullptr, &error));

    return Status();
}

COMMAND(InitLevel::wallet, SpendTransfer, "spend-transfer")
{
    if (argc != 5)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... spend-transfer <user> <pass> <wallet> <wallet-dest> <amount>");
    const char *dest = argv[3];
    int amount = atoi(argv[4]);

    Session sessionDest = session;
    sessionDest.uuid = dest;
    WatcherThread threadDest;
    ABC_CHECK(threadDest.init(sessionDest));

    WatcherThread thread;
    ABC_CHECK(thread.init(session));

    AutoFree<tABC_SpendTarget, ABC_SpendTargetFree> pSpend;
    ABC_CHECK_OLD(ABC_SpendNewTransfer(session.username, dest, amount,
                                       &pSpend.get(), &error));
    std::cout << "Sending " << pSpend->amount << " satoshis to " << pSpend->szName
              << std::endl;

    AutoString szTxId;
    ABC_CHECK_OLD(ABC_SpendApprove(session.username, session.uuid, pSpend,
                                   &szTxId.get(), &error));
    std::cout << "Transaction id: " << szTxId.get() << std::endl;

    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username, session.password,
                                     session.uuid, syncCallback, nullptr, &error));
    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username, session.password, dest,
                                     syncCallback, nullptr, &error));

    return Status();
}

COMMAND(InitLevel::wallet, SpendInternal, "spend-internal")
{
    if (argc != 5)
        return ABC_ERROR(ABC_CC_Error,
                         "usage: ... spend-internal <user> <pass> <wallet> <address> <amount>");
    const char *address = argv[3];
    int amount = atoi(argv[4]);

    WatcherThread thread;
    ABC_CHECK(thread.init(session));

    AutoFree<tABC_SpendTarget, ABC_SpendTargetFree> pSpend;
    ABC_CHECK_OLD(ABC_SpendNewInternal(address, nullptr, nullptr, nullptr, amount,
                                       &pSpend.get(), &error));
    std::cout << "Sending " << pSpend->amount << " satoshis to " << pSpend->szName
              << std::endl;

    AutoString szTxId;
    ABC_CHECK_OLD(ABC_SpendApprove(session.username, session.uuid, pSpend,
                                   &szTxId.get(), &error));
    std::cout << "Transaction id: " << szTxId.get() << std::endl;

    ABC_CHECK_OLD(ABC_DataSyncWallet(session.username, session.password,
                                     session.uuid, syncCallback, nullptr, &error));

    return Status();
}
