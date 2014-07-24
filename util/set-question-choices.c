#include "common.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 6)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass> <rqs> <ras>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], NULL, 0, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_SetAccountRecoveryQuestions(argv[2], argv[3], argv[4], argv[5], NULL, NULL, &error));
    return 0;
}
