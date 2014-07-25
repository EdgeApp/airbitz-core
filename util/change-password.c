#include "common.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    char *szPIN = NULL;

    if (argc != 7)
    {
        fprintf(stderr, "usage: %s <dir> <pw|ra> <user> <pass|ra> <new-pass> <pin>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], NULL, 0, seed, sizeof(seed), &error));
    if (strncmp(argv[2], "pw", 2) == 0)
    {
        MAIN_CHECK(ABC_ChangePassword(argv[3], argv[4], argv[5], argv[6], NULL, NULL, &error));
    }
    else
    {
        MAIN_CHECK(ABC_ChangePasswordWithRecoveryAnswers(argv[3], argv[4], argv[5], argv[6], NULL, NULL, &error));
    }
    MAIN_CHECK(ABC_ClearKeyCache(&error));
    return 0;
}
