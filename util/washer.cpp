#include "common.h"
#include "ABC.h"
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>

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
    } else if (pInfo->eventType == ABC_AsyncEventType_ExchangeRateUpdate) {
        printf("ABC_AsyncEventType_ExchangeRateUpdate\n");
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
    tABC_Error error;
    while (running) {
        ABC_DataSyncAll(gszUserName, gszPassword, NULL, NULL, &error);
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
    if (maxSatoshi < 0) {
        return;
    }
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
            PrintError(cc, &error);
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

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, sig_handler);

    char *szDir = argv[1];
    gszUserName = argv[2];
    gszPassword = argv[3];

    pthread_t data_thread;
    char **szUUIDs = NULL;
    tThread **threads = NULL;
    unsigned int count = 0;

    MAIN_CHECK(ABC_Initialize(szDir, CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_SignIn(gszUserName, gszPassword, NULL, NULL, &error));
    MAIN_CHECK(ABC_GetWalletUUIDs(gszUserName, gszPassword, &szUUIDs, &count, &error));

    threads = (tThread**)malloc(sizeof(tThread *) * count);

    if (!pthread_create(&data_thread, NULL, data_loop, NULL))
    {
        pthread_detach(data_thread);
    }

    for (unsigned i = 0; i < count; ++i) {
        char *szUUID = szUUIDs[i];
        MAIN_CHECK(ABC_WatcherStart(gszUserName, gszPassword, szUUID, &error));

        tThread *thread = (tThread*)malloc(sizeof(tThread));
        thread->szUUID = strdup(szUUID);
        threads[i] = thread;
        if (!pthread_create(&(thread->watcher_thread), NULL, watcher_loop, szUUID))
        {
            pthread_detach(thread->watcher_thread);
        }

        MAIN_CHECK(ABC_WatchAddresses(gszUserName, gszPassword, szUUID, &error));
        MAIN_CHECK(ABC_WatcherConnect(szUUID, &error));
    }
    main_loop();
    for (unsigned i = 0; i < count; ++i) {
        tThread *thread = threads[i];
        MAIN_CHECK(ABC_WatcherStop(thread->szUUID, &error));
        pthread_join(thread->watcher_thread, NULL);
        free(thread->szUUID);
        free(thread);
        thread = NULL;
    }
    free(threads);

    pthread_join(data_thread, NULL);

    MAIN_CHECK(ABC_ClearKeyCache(&error));
    return 0;
}
