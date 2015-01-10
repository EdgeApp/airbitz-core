#include "common.h"
#include <stdio.h>

#include "util/ABC_Util.h"

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    tABC_RequestResults results;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <wallet-name>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_CreateWallet(argv[2], argv[3], argv[4], CURRENCY_NUM_USD,
                                0, NULL, &results, &error));
    return 0;
}
