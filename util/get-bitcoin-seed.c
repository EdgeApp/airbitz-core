#include "common.h"
#include <stdio.h>

#include <ABC_Util.h>
#include <ABC_Wallet.h>
#include <ABC_Crypto.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_U08Buf data;
    char *szSeed;

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <wallet-name>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], NULL, 0, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_WalletGetBitcoinPrivateSeed(argv[2], argv[3], argv[4], &data, &error));
    MAIN_CHECK(ABC_CryptoHexEncode(data, &szSeed, &error));
    printf("%s\n", szSeed);

    ABC_BUF_FREE(data);
    ABC_FREE_STR(szSeed);

    return 0;
}
