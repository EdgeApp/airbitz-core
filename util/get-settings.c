#include "common.h"
#include "ABC_Util.h"

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
    MAIN_CHECK(ABC_LoadAccountSettings(argv[2], argv[3], &pSettings, &error));

    printf("First name: %s\n", pSettings->szFirstName ? pSettings->szFirstName : "(none)");
    printf("Last name: %s\n", pSettings->szLastName ? pSettings->szLastName : "(none)");
    printf("Nickname: %s\n", pSettings->szNickname ? pSettings->szNickname : "(none)");
    printf("List name on payments: %s\n", pSettings->bNameOnPayments ? "yes" : "no");
    printf("Minutes before auto logout: %d\n", pSettings->minutesAutoLogout);
    printf("Language: %s\n", pSettings->szLanguage);
    printf("Currency num: %d\n", pSettings->currencyNum);
    printf("Advanced features: %s\n", pSettings->bAdvancedFeatures ? "yes" : "no");
    printf("Denomination satoshi: %ld\n", pSettings->bitcoinDenomination.satoshi);
    printf("Denomination id: %d\n", pSettings->bitcoinDenomination.denominationType);
    printf("Exchange rate sources:\n");
    for (unsigned i = 0; i < pSettings->exchangeRateSources.numSources; i++)
    {
        printf("\tcurrency: %d\tsource: %s\n",
            pSettings->exchangeRateSources.aSources[i]->currencyNum,
            pSettings->exchangeRateSources.aSources[i]->szSource);
    }

    MAIN_CHECK(ABC_ClearKeyCache(&error));
    ABC_FreeAccountSettings(pSettings);
    return 0;
}
