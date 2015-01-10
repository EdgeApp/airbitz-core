#include "common.h"
#include <stdio.h>

#include "LoginShim.hpp"
#include "util/Crypto.hpp"

using namespace abcd;

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_SyncKeys *pKeys = NULL;
    tABC_U08Buf data;
    char *szContents = NULL;
    char *szEncrypted = NULL;

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <filename>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));

    MAIN_CHECK(ABC_LoginShimGetSyncKeys(argv[2], argv[3], &pKeys, &error));

    szContents = Slurp(argv[4]);

    if (szContents)
    {
        ABC_BUF_SET_PTR(data, (unsigned char *) szContents, strlen(szContents));

        MAIN_CHECK(ABC_CryptoEncryptJSONString(
                    data, pKeys->MK, ABC_CryptoType_AES256, &szEncrypted, &error));
        printf("%s\n", szEncrypted);
    }

    ABC_SyncFreeKeys(pKeys);
    ABC_FREE_STR(szEncrypted);
    ABC_FREE_STR(szContents);

    return 0;
}
