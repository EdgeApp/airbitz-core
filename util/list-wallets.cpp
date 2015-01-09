#include "common.h"
#include <stdio.h>

#include "ABC_Account.h"
#include "ABC_LoginShim.h"
#include "ABC_Wallet.h"
#include "util/ABC_Crypto.h"
#include "util/ABC_FileIO.h"
#include "util/ABC_Util.h"

using namespace abcd;

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_SyncKeys *pKeys = NULL;
    char **aszUUIDs;
    unsigned int count = 0;

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass>\n", argv[0]);
        return 1;
    }

    // Setup:
    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_LoginShimGetSyncKeys(argv[2], argv[3], &pKeys, &error));
    MAIN_CHECK(ABC_DataSyncAll(argv[2], argv[3], NULL, NULL, &error));

    // Iterate over wallets:
    MAIN_CHECK(ABC_GetWalletUUIDs(argv[2], argv[3],
        &aszUUIDs, &count, &error));
    for (unsigned i = 0; i < count; ++i)
    {
        // Print the UUID:
        printf("%s: ", aszUUIDs[i]);

        // Get wallet name filename:
        char *szDir;
        char szFilename[ABC_FILEIO_MAX_PATH_LENGTH];
        MAIN_CHECK(ABC_WalletGetDirName(&szDir, aszUUIDs[i], &error));
        snprintf(szFilename, sizeof(szFilename),
            "%s/sync/WalletName.json", szDir);

        // Print wallet name:
        tABC_U08Buf data;
        tABC_AccountWalletInfo info = {0};
        MAIN_CHECK(ABC_AccountWalletLoad(pKeys, aszUUIDs[i], &info, &error));
        if (ABC_CC_Ok ==ABC_CryptoDecryptJSONFile(szFilename, info.MK, &data, &error))
        {
            fwrite(data.p, data.end - data.p, 1, stdout);
            printf("\n");
            ABC_BUF_FREE(data);
        }
        ABC_AccountWalletInfoFree(&info);
    }
    printf("\n");

    // Clean up:
    ABC_SyncFreeKeys(pKeys);
    ABC_UtilFreeStringArray(aszUUIDs, count);
    return 0;
}
