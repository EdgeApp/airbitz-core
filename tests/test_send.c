#define RED  "\x1B[31m"
#define GRN  "\x1B[32m"
#define NRM  "\x1B[0m"

#include "ABC.h"
#include "ABC_Tx.h"
#include "ABC_Bridge.h"
#include "util/ABC_Util.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <semaphore.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_WAIT_ON_CB(codeBlock) \
    { \
        sem_wait(&cb_sem); \
        codeBlock \
        if (Error.code != ABC_CC_Ok) { \
            sem_post(&cb_sem); \
        } else { \
            sem_wait(&cb_sem); \
            sem_post(&cb_sem); \
        } \
    }

static char *USERNAME;
static char *PASSWORD;
static char *WALLET_UUID;
static const char *WALLET_NAME = "My Wallet";
static sem_t cb_sem;

static char NET_ADDR[512];
static char NET_PRIV[512];
static char NET_RECV_ADDR[512];

static void printABC_Error(const tABC_Error *pError)
{
    if (pError)
    {
        if (pError->code != ABC_CC_Ok)
        {
            printf("%sFailed!%s\n", RED, NRM);
            printf("Code: %d, Desc: %s, Func: %s, File: %s, Line: %d\n",
                   pError->code,
                   pError->szDescription,
                   pError->szSourceFunc,
                   pError->szSourceFile,
                   pError->nSourceLine
                   );
        } else {
            printf("%sPassed!%s\n", GRN, NRM);
        }
        printf("\n");
    }
}

static void ABC_BitCoin_Event_Callback(const tABC_AsyncBitCoinInfo *pInfo)
{
    printf("ABC_BitCoin_Event_Callback\n");
    if (pInfo->eventType == ABC_AsyncEventType_IncomingBitCoin)
    {
        printf("Received an Incoming Bitcoin\n");
    }
}

static void ABC_Request_Callback(const tABC_RequestResults *pResults)
{
    printf("ABC_Request_Callback\n");
    printABC_Error(&(pResults->errorInfo));

    /* Release semaphore, and continue with tests */
    sem_post(&cb_sem);
}

static void ABC_WalletCreate_Callback(const tABC_RequestResults *pResults)
{
    size_t slen = strlen((char *) pResults->pRetData) + 1;
    WALLET_UUID = (char *) malloc(sizeof(char) * slen);
    snprintf(WALLET_UUID, slen, "%s", (char *) pResults->pRetData);

    printABC_Error(&(pResults->errorInfo));

    /* Release semaphore, and continue with tests */
    sem_post(&cb_sem);
}

static void create_credentials()
{
    size_t sSize = 20;
    USERNAME = (char *) malloc(sSize * sizeof(char));
    PASSWORD = (char *) malloc(sSize * sizeof(char));

    snprintf(USERNAME, sSize, "login%ld", time(NULL));
    snprintf(PASSWORD, sSize, "pass%ld", time(NULL));
}

static void test_createwallet()
{
    tABC_Error Error;
    TEST_WAIT_ON_CB(
        ABC_CreateWallet(USERNAME,
                        PASSWORD,
                        WALLET_NAME,
                        840, // currency num
                        0, // attributes
                        ABC_WalletCreate_Callback,
                        NULL,
                        &Error);
        printABC_Error(&Error);
    )
}

static void destroy_credentials()
{
    free(USERNAME);
    free(PASSWORD);
    free(WALLET_UUID);
}

static void test_receive_request_args(char *n, char *c, char *notes)
{
    tABC_Error Error;
    tABC_TxDetails Details;
    Details.amountSatoshi = 100;
    Details.amountCurrency = 8.8;
    Details.szName = n;
    Details.szCategory = c;
    Details.szNotes = notes;
    Details.attributes = 0x1;

    char *szRequestID = NULL;

    ABC_CreateReceiveRequest(USERNAME, PASSWORD, WALLET_UUID,
                             &Details, &szRequestID, &Error);
    printABC_Error(&Error);

    if (szRequestID)
    {
        free(szRequestID);
    }
}

static void test_initialize()
{
    tABC_Error Error;
    size_t size = 4;
    const char *datapath = "./tmp";
    struct stat st = {0 };
    if (stat(datapath, &st) == -1)
    {
        mkdir(datapath, 0770);
    }
    char unsigned seed[] = "abcd";
    ABC_Initialize(datapath,
                   ABC_BitCoin_Event_Callback,
                   NULL,
                   seed,
                   size,
                   &Error);
    printABC_Error(&Error);
}

static void test_createaccount()
{
    tABC_Error Error;
    TEST_WAIT_ON_CB(
        ABC_CreateAccount(USERNAME,
                        PASSWORD,
                        (char *)"1234",
                        ABC_Request_Callback,
                        NULL,
                        &Error);
        printABC_Error(&Error);
    )
}

static void sleepy_poll()
{
    int i = 0;
    while (i < 5)
    {
        printf("Test Case Wait, expecting watcher output...%d\n", i);
        sleep(20);
        i++;
    }
}

static void strtrim(char *s)
{
    size_t l = strlen(s) - 1;
    while (l > 0)
    {
        if (s[l]  == '\n' || s[l] == ' ')
        {
            s[l] = '\0';
        }
        l--;
    }
}

static void strread(char *s, size_t sz)
{
    memset(s, '\0', 512);
    fgets(s, 512, stdin);
    strtrim(s);
}

int main(int argc, const char *argv[])
{
    tABC_Error pError;
    tABC_TxSendInfo sendInfo;

    printf("Give me a private key num num num:\n");
    strread(NET_PRIV, 512);
    printf("Using '%s'\n", NET_PRIV);

    printf("Give me a public key to source funds from:\n");
    strread(NET_ADDR, 512);
    printf("Using '%s'\n", NET_ADDR);

    printf("Who to sends funds to?\n");
    strread(NET_RECV_ADDR, 512);
    printf("Using '%s'\n", NET_RECV_ADDR);

    sem_init(&cb_sem, 0, 1);

    create_credentials();
    test_initialize();
    test_createaccount();
    test_createwallet();
    /* Update account information */
    if (ABC_AccountServerUpdateGeneralInfo(&pError) != ABC_CC_Ok)
    {
        printf("Failed Server Info Update\n");
    }
    /* Start the Watcher */
    if (ABC_WatcherStart(USERNAME, PASSWORD, WALLET_UUID, &pError) != ABC_CC_Ok)
    {
        printf("Failed Watch Start\n");
    }
    /* Add a public address */
    if (ABC_BridgeWatchAddr(USERNAME, PASSWORD, WALLET_UUID,
                            NET_ADDR, true, &pError) != ABC_CC_Ok)
    {
    }
    sleepy_poll();

    sendInfo.szUserName = USERNAME;
    sendInfo.szPassword = PASSWORD;
    sendInfo.szWalletUUID = WALLET_UUID;
    sendInfo.szDestAddress = NET_RECV_ADDR;
    sendInfo.pDetails = malloc(sizeof(tABC_TxDetails));
    sendInfo.pDetails->amountSatoshi = 5000;
    sendInfo.pDetails->amountFeesAirbitzSatoshi = 0;
    sendInfo.pDetails->amountFeesMinersSatoshi = 0;

    char **addresses = malloc(sizeof(char *));
    addresses[0] = NET_ADDR;

    char **privAddresses = malloc(sizeof(char *));
    privAddresses[0] = NET_PRIV;

    tABC_UnsignedTx pUtx;
    if (ABC_BridgeTxMake(&sendInfo,
                         addresses, 1,
                         &pUtx,
                         &pError) != ABC_CC_Ok)
    {
        printf("Failed to mktx\n");
    }
    else {
        printf("Fees: %ld\n", pUtx.fees);

        if (ABC_BridgeTxSignSend(&sendInfo,
                                 privAddresses, 1,
                                 &pUtx, &pError) != ABC_CC_Ok)
        {
            printf("Failed to mktx\n");
        }
        sleepy_poll();
    }

    printf("Stopping Watcher\n");
    if (ABC_WatcherStop(WALLET_UUID, &pError) != ABC_CC_Ok)
    {
        printf("Failed Watcher Stop!\n");
    }
    destroy_credentials();
    sem_destroy(&cb_sem);
    return 0;
}

