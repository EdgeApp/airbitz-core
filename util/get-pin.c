#include "common.h"
#include "ABC_Util.h"

#include <stdio.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    char *szPIN = NULL;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], NULL, 0, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_GetPIN(argv[2], argv[3], &szPIN, &error));
    printf("%s\n", szPIN);
    if (szPIN)
    {
        free(szPIN);
    }
    MAIN_CHECK(ABC_ClearKeyCache(&error));

    return 0;
}
