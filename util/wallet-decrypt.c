#include "common.h"
#include <stdio.h>

#include "ABC_Login.h"
#include "ABC_Account.h"
#include "util/ABC_Crypto.h"

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_SyncKeys *pKeys = NULL;
    tABC_AccountWalletInfo info;
    tABC_U08Buf data;

    if (argc != 6)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <uuid> <filepath>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));

    MAIN_CHECK(ABC_LoginGetSyncKeys(argv[2], argv[3], &pKeys, &error));
    MAIN_CHECK(ABC_AccountWalletLoad(pKeys, argv[4], &info, &error));

    MAIN_CHECK(ABC_CryptoDecryptJSONFile(argv[5], info.MK, &data, &error));
    fwrite(data.p, data.end - data.p, 1, stdout);
    printf("\n");

    ABC_SyncFreeKeys(pKeys);
    ABC_AccountWalletInfoFree(&info);
    ABC_BUF_FREE(data);

    return 0;
}
