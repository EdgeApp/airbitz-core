#include "common.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    char *szQuestions = NULL;
    bool bValid = false;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <dir> <user> <ras>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], NULL, 0, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_GetRecoveryQuestions(argv[2], &szQuestions, &error));

    printf("%s\n", szQuestions);
    MAIN_CHECK(ABC_CheckRecoveryAnswers(argv[2], argv[3], &bValid, &error));
    printf("%s\n", bValid ? "Valid!" : "Invalid!");
    if (szQuestions)
    {
        free(szQuestions);
    }
    return 0;
}
