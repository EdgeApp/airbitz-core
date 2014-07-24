#include "common.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};
    char *szPIN = NULL;

    if (argc != 5)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <new-pass>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], NULL, 0, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_GetPIN(argv[2], argv[3], &szPIN, &error));
    MAIN_CHECK(ABC_ChangePassword(argv[2], argv[3], argv[4], szPIN, NULL, NULL, &error));
    MAIN_CHECK(ABC_ClearKeyCache(&error));
    return 0;
}
