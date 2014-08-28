#include "common.h"
#include "ABC_Util.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    tABC_AccountSettings *pSettings = NULL;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <nickname>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_LoadAccountSettings(argv[2], argv[3], &pSettings, &error));
    free(pSettings->szNickname);
    pSettings->szNickname = strdup(argv[4]);
    MAIN_CHECK(ABC_UpdateAccountSettings(argv[2], argv[3], pSettings, &error));

    MAIN_CHECK(ABC_ClearKeyCache(&error));
    ABC_FreeAccountSettings(pSettings);
    return 0;
}
