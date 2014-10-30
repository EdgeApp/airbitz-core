#include "common.h"
#include <iostream>
#include <stdio.h>

#include "ABC_LoginShim.h"
#include "ABC_Wallet.h"
#include "util/ABC_Util.h"
#include <wallet/wallet.hpp>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char rngseed[] = {1, 2, 3};
    tABC_SyncKeys *pKeys = NULL;
    tABC_U08Buf data;

    if (argc != 6)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <wallet-name> <count>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, rngseed, sizeof(rngseed), &error));
    MAIN_CHECK(ABC_LoginShimGetSyncKeys(argv[2], argv[3], &pKeys, &error));
    MAIN_CHECK(ABC_WalletGetBitcoinPrivateSeed(ABC_WalletID(pKeys, argv[4]), &data, &error));

    libbitcoin::data_chunk seed(data.p, data.end);
    libwallet::hd_private_key m(seed);
    libwallet::hd_private_key m0 = m.generate_private_key(0);
    libwallet::hd_private_key m00 = m0.generate_private_key(0);
    long max = strtol(argv[5], 0, 10);
    for (int i = 0; i < max; ++i)
    {
        libwallet::hd_private_key m00n = m00.generate_private_key(i);
        std::cout << "watch " << m00n.address().encoded() << std::endl;
    }

    ABC_SyncFreeKeys(pKeys);
    ABC_BUF_FREE(data);

    return 0;
}
