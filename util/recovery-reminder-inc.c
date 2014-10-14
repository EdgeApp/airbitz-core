#include "common.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    tABC_AccountSettings *pSettings = NULL;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_SignIn(argv[2], argv[3], NULL, NULL, &error));
    MAIN_CHECK(ABC_LoadAccountSettings(argv[2], argv[3], &pSettings, &error));

    printf("Old Reminder Count: %d\n", pSettings->recoveryReminderCount);
    pSettings->recoveryReminderCount++;
    printf("New Reminder Count: %d\n", pSettings->recoveryReminderCount);

    MAIN_CHECK(ABC_UpdateAccountSettings(argv[2], argv[3], pSettings, &error));

    ABC_FreeAccountSettings(pSettings);
    MAIN_CHECK(ABC_ClearKeyCache(&error));

    return 0;
}
