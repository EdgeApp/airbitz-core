#include "common.h"
#include <stdio.h>

#include <ABC_Bridge.h>
#include <ABC_Util.h>
#include <ABC_Wallet.h>
#include <ABC_Crypto.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_U08Buf data;

    if (argc != 6)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <wallet-name> <addr>\n", argv[0]);
        return 1;
    }

    char *szMatchAddr = argv[5];

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, NULL, 0, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_WalletGetBitcoinPrivateSeed(argv[2], argv[3], argv[4], &data, &error));

    for (int32_t i = 0, c = 0; i < 4294967296; ++i, ++c)
    {
        char *szPubAddress = NULL;
        ABC_BridgeGetBitcoinPubAddress(&szPubAddress, data, i, NULL);
        if (strncmp(szPubAddress, szMatchAddr, strlen(szMatchAddr)) == 0)
        {
            printf("Found %s at %d\n", szMatchAddr, i);
            ABC_FREE(szPubAddress);
            break;
        }
        ABC_FREE(szPubAddress);
        if (c == 100000)
        {
            printf("%d\n", i);
            c = 0;
        }
    }

    ABC_BUF_FREE(data);

    return 0;
}
