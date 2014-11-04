#include "common.h"
#include "ABC_LoginPassword.h"
#include "ABC_LoginServer.h"
#include <stdio.h>
#include <time.h>

#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pin>\n", argv[0]);
        return 1;
    }
    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));

    MAIN_CHECK(ABC_PinLogin(argv[2], argv[3], &error));

    return 0;
}
