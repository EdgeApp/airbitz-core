#include "common.h"
#include <stdio.h>

#include <ABC_Login.h>
#include <ABC_Crypto.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_SyncKeys *pKeys = NULL;
    char szFilename[2048];
    tABC_U08Buf data;

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <filename>\n", argv[0]);
        fprintf(stderr, "note: The filename is account-relative.\n");
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], NULL, 0, seed, sizeof(seed), &error));

    MAIN_CHECK(ABC_LoginGetSyncKeys(argv[2], argv[3], &pKeys, &error));
    sprintf(szFilename, "%s/%s", pKeys->szSyncDir, argv[4]);

    MAIN_CHECK(ABC_CryptoDecryptJSONFile(szFilename, pKeys->MK, &data, &error));
    fwrite(data.p, data.end - data.p, 1, stdout);
    printf("\n");

    ABC_SyncFreeKeys(pKeys);
    ABC_BUF_FREE(data);

    return 0;
}
