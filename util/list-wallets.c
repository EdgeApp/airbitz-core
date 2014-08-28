#include "common.h"
#include "ABC_Util.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_WalletInfo **paWalletInfo = NULL;
    unsigned int count = 0;

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_GetWallets(argv[2], argv[3],
                           &paWalletInfo,
                           &count,
                           &error));
    for (int i = 0; i < count; ++i)
    {
        tABC_WalletInfo *wallet = paWalletInfo[i];
        printf("%s - %s\n", wallet->szName, wallet->szUUID);
    }
    printf("\n");
    ABC_FreeWalletInfoArray(paWalletInfo, count);
    return 0;
}
