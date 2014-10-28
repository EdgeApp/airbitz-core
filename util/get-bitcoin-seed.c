#include "common.h"
#include <stdio.h>

#include "ABC_Login.h"
#include "ABC_Wallet.h"
#include "util/ABC_Crypto.h"
#include "util/ABC_Util.h"

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_SyncKeys *pKeys = NULL;
    tABC_U08Buf data;
    char *szSeed;

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <wallet-name>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));

    MAIN_CHECK(ABC_LoginGetSyncKeys(argv[2], argv[3], &pKeys, &error));
    MAIN_CHECK(ABC_WalletGetBitcoinPrivateSeed(ABC_WalletID(pKeys, argv[4]), &data, &error));
    MAIN_CHECK(ABC_CryptoHexEncode(data, &szSeed, &error));
    printf("%s\n", szSeed);

    ABC_SyncFreeKeys(pKeys);
    ABC_BUF_FREE(data);
    ABC_FREE_STR(szSeed);

    return 0;
}
