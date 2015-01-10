#include "common.h"
#include <stdio.h>

#include "util/Util.hpp"

using namespace abcd;

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    char **aszCategories;
    unsigned count;
    unsigned char seed[] = {1, 2, 3};

    if (argc != 4)
    {
        fprintf(stderr, "usage: %s <dir> <user> <pass>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], CA_CERT, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_GetCategories(argv[2], argv[3], &aszCategories, &count, &error));

    printf("Categories:\n");
    for (unsigned i = 0; i < count; ++i)
    {
        printf("\t%s\n", aszCategories[i]);
    }

    MAIN_CHECK(ABC_ClearKeyCache(&error));
    ABC_UtilFreeStringArray(aszCategories, count);
    return 0;
}
