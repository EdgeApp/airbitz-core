#include "common.h"
#include <stdio.h>

#include <ABC_Login.h>
#include <ABC_Account.h>
#include <ABC_Crypto.h>
#include <ABC_Wallet.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_SyncKeys *pKeys = NULL;
    tABC_AccountWalletInfo info;
    tABC_U08Buf data = ABC_BUF_NULL;
    char *szContents = NULL;
    char *szEncrypted = NULL;

    if (argc != 6)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <uuid> <filepath>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, NULL, 0, seed, sizeof(seed), &error));

    MAIN_CHECK(ABC_LoginGetSyncKeys(argv[2], argv[3], &pKeys, &error));
    MAIN_CHECK(ABC_AccountWalletLoad(pKeys, argv[4], &info, &error));

    szContents = Slurp(argv[5]);
    if (szContents)
    {
        ABC_BUF_SET_PTR(data, (unsigned char *) szContents, strlen(szContents));

        MAIN_CHECK(ABC_CryptoEncryptJSONString(
                    data, info.MK, ABC_CryptoType_AES256, &szEncrypted, &error));
        printf("%s\n", szEncrypted);
    }

    ABC_SyncFreeKeys(pKeys);
    ABC_AccountWalletInfoFree(&info);
    ABC_FREE_STR(szContents);
    ABC_FREE_STR(szEncrypted);

    return 0;
}
