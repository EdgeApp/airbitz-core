#include "common.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};

    double secondsToCrack;
    unsigned int count = 0;
    tABC_PasswordRule **aRules = NULL;

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <dir> <pass>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_CheckPassword(argv[2], &secondsToCrack, &aRules, &count, &error));
    for (unsigned i = 0; i < count; ++i)
    {
        printf("%s: %d\n", aRules[i]->szDescription, aRules[i]->bPassed);
    }
    printf("Time to Crack: %f\n", secondsToCrack);
    ABC_FreePasswordRuleArray(aRules, count);
    MAIN_CHECK(ABC_ClearKeyCache(&error));

    return 0;
}
