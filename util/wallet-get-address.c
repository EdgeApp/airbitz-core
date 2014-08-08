#include "common.h"
#include <stdio.h>

#include <ABC_Account.h>
#include <ABC_Wallet.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <uuid>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, NULL, 0, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_SignIn(argv[2], argv[3], NULL, NULL, &error));

    tABC_TxDetails details;
    details.szName = "";
    details.szCategory = "";
    details.szNotes = "";
    details.attributes = 0x0;
    details.bizId = 0x0;
    details.attributes = 0x0;
    details.bizId = 0;
    details.amountSatoshi = 0;
    details.amountCurrency = 0;
    details.amountFeesAirbitzSatoshi = 0;
    details.amountFeesMinersSatoshi = 0;


    char *szRequestID = NULL;
    char *szAddress = NULL;
    printf("starting...");
    MAIN_CHECK(ABC_CreateReceiveRequest(argv[2], argv[3], argv[4],
                    &details, &szRequestID, &error));

    MAIN_CHECK(ABC_GetRequestAddress(argv[2], argv[3], argv[4],
                    szRequestID, &szAddress, &error));
    printf("finishing...");

    printf("Address: %s\n", szAddress);

    ABC_FREE_STR(szRequestID);
    ABC_FREE_STR(szAddress);

    return 0;
}
