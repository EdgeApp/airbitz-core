#include "common.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_SignIn(argv[2], argv[3], NULL, NULL, &error));
    MAIN_CHECK(ABC_UploadLogs(argv[2], argv[3], &error));
    MAIN_CHECK(ABC_ClearKeyCache(&error));
    return 0;
}
