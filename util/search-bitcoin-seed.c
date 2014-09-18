#include "common.h"
#include <stdio.h>

#include "ABC_Bridge.h"
#include "ABC_Wallet.h"
#include "util/ABC_Crypto.h"
#include "util/ABC_Util.h"

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_U08Buf data;

    if (argc != 8)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <wallet-name> <addr> <start> <end>\n", argv[0]);
        return 1;
    }
    long start = atol(argv[6]);
    long end = atol(argv[7]);

    char *szMatchAddr = argv[5];

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_WalletGetBitcoinPrivateSeed(argv[2], argv[3], argv[4], &data, &error));

    for (long i = start, c = 0; i <= end; i++, ++c)
    {
        char *szPubAddress = NULL;
        ABC_BridgeGetBitcoinPubAddress(&szPubAddress, data, (int32_t) i, NULL);
        if (strncmp(szPubAddress, szMatchAddr, strlen(szMatchAddr)) == 0)
        {
            printf("Found %s at %ld\n", szMatchAddr, i);
            ABC_FREE(szPubAddress);
            break;
        }
        ABC_FREE(szPubAddress);
        if (c == 100000)
        {
            printf("%ld\n", i);
            c = 0;
        }
    }

    ABC_BUF_FREE(data);

    return 0;
}
