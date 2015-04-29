/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Command.hpp"
#include "Util.hpp"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <iostream>

using namespace abcd;

typedef struct sThread
{
    char *szUUID;
    pthread_t watcher_thread;
} tThread;

#define MIN_BALANCE 10000

static bool running = true;
static char *gszUserName;
static char *gszPassword;

void sig_handler(int dummy)
{
    running = false;
}

void async_callback(const tABC_AsyncBitCoinInfo *pInfo)
{
    if (pInfo->eventType == ABC_AsyncEventType_IncomingBitCoin) {
        printf("ABC_AsyncEventType_IncomingBitCoin\n");
    } else if (pInfo->eventType == ABC_AsyncEventType_BlockHeightChange) {
        printf("ABC_AsyncEventType_BlockHeightChange\n");
    } else if (pInfo->eventType == ABC_AsyncEventType_DataSyncUpdate) {
        printf("ABC_AsyncEventType_DataSyncUpdate\n");
    } else if (pInfo->eventType == ABC_AsyncEventType_RemotePasswordChange) {
        printf("ABC_AsyncEventType_RemotePasswordChange\n");
    }
}

void *watcher_loop(void *pData)
{
    tABC_Error error;
    char *szUUID = (char *)pData;
    ABC_WatcherLoop(szUUID, async_callback, NULL, &error);
    return NULL;
}

void *data_loop(void *pData)
{
    auto account = static_cast<Account *>(pData);
    while (running) {
        syncAll(*account);
        sleep(5);
    }

    return NULL;
}

void send_tx(tABC_WalletInfo *wallet)
{
    printf("send_tx(%ld)\n", wallet->balanceSatoshi);
    if (wallet->balanceSatoshi < MIN_BALANCE) {
        return;
    }
    tABC_Error error;
    uint64_t maxSatoshi = 0;
    char *szID = NULL;
    char *szAddress = NULL;
    tABC_TxDetails details;
    memset(&details, 0, sizeof(tABC_TxDetails));
    details.amountSatoshi = 0;
    details.amountCurrency = 0;
    details.amountFeesAirbitzSatoshi = 0;
    details.amountFeesMinersSatoshi = 0;
    details.szName = const_cast<char*>("");
    details.szCategory = const_cast<char*>("");
    details.szNotes = const_cast<char*>("");
    details.attributes = 0x2;

    // Create new receive request
    ABC_CreateReceiveRequest(gszUserName, gszPassword, wallet->szUUID,
                             &details, &szID, &error);

    // Fetch the address
    ABC_GetRequestAddress(gszUserName, gszPassword, wallet->szUUID,
                          szID, &szAddress, &error);

    // Get the max spendable
    ABC_MaxSpendable(gszUserName, gszPassword, wallet->szUUID,
                     szAddress, false, &maxSatoshi, NULL);
    printf("Dest Address: %s\n", szAddress);
    printf("Balance: %ld\n", wallet->balanceSatoshi);
    printf("Max Spendable: %ld\n", maxSatoshi);

    // break max spendable a part
    int num = maxSatoshi / MIN_BALANCE;
    for (int i = 0; i < num; ++i) {
        ABC_MaxSpendable(gszUserName, gszPassword, wallet->szUUID,
                        szAddress, false, &maxSatoshi, NULL);
        if (maxSatoshi > MIN_BALANCE)
        {
            details.amountSatoshi = MIN_BALANCE;

            // Send Tx
            char *szTxId = NULL;
            tABC_Error error;
            tABC_CC cc = ABC_InitiateSendRequest(gszUserName, gszPassword,
                                    wallet->szUUID, szAddress,
                                    &details, &szTxId, &error);
            if (cc != ABC_CC_Ok)
                std::cerr << Status::fromError(error) << std::endl;
            free(szTxId);
        }
    }
}

void main_loop()
{
    while (running) {
        tABC_Error error;
        tABC_WalletInfo **paWalletInfo = NULL;
        unsigned int count = 0;
        ABC_GetWallets(gszUserName, gszPassword, &paWalletInfo, &count, &error);
        for (unsigned i = 0; i < count; ++i) {
            send_tx(paWalletInfo[i]);
        }
        ABC_FreeWalletInfoArray(paWalletInfo, count);

        sleep(5);
    }
}

COMMAND(InitLevel::account, Washer, "washer")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, "usage: ... washer <user> <pass>");
    gszUserName = argv[0];
    gszPassword = argv[1];

    char **szUUIDs = NULL;
    unsigned int count = 0;
    ABC_CHECK_OLD(ABC_GetWalletUUIDs(gszUserName, gszPassword, &szUUIDs, &count, &error));

    signal(SIGINT, sig_handler);

    pthread_t data_thread;
    if (!pthread_create(&data_thread, NULL, data_loop, session.account.get()))
    {
        pthread_detach(data_thread);
    }

    tThread **threads = NULL;
    threads = (tThread**)malloc(sizeof(tThread *) * count);
    for (unsigned i = 0; i < count; ++i) {
        char *szUUID = szUUIDs[i];
        ABC_CHECK_OLD(ABC_WatcherStart(gszUserName, gszPassword, szUUID, &error));

        tThread *thread = (tThread*)malloc(sizeof(tThread));
        thread->szUUID = strdup(szUUID);
        threads[i] = thread;
        if (!pthread_create(&(thread->watcher_thread), NULL, watcher_loop, szUUID))
        {
            pthread_detach(thread->watcher_thread);
        }

        ABC_CHECK_OLD(ABC_WatchAddresses(gszUserName, gszPassword, szUUID, &error));
        ABC_CHECK_OLD(ABC_WatcherConnect(szUUID, &error));
    }
    main_loop();
    for (unsigned i = 0; i < count; ++i) {
        tThread *thread = threads[i];
        ABC_CHECK_OLD(ABC_WatcherStop(thread->szUUID, &error));
        pthread_join(thread->watcher_thread, NULL);
        free(thread->szUUID);
        free(thread);
        thread = NULL;
    }
    free(threads);

    pthread_join(data_thread, NULL);

    return Status();
}
