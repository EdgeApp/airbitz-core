#include "common.h"
#include <stdio.h>

#include "Login.hpp"
#include "Wallet.hpp"
#include "util/Crypto.hpp"
#include "util/Util.hpp"

using namespace abcd;

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    tABC_WalletInfo *pInfo = NULL;

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <wallet-uuid>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_SignIn(argv[2], argv[3], NULL, NULL, &error));
    MAIN_CHECK(ABC_GetWalletInfo(argv[2], argv[3], argv[4], &pInfo, &error));

    ABC_WalletFreeInfo(pInfo);

    return 0;
}
