#include "common.h"
#include <stdio.h>

int main(int argc, char *argv[])
{
    tABC_CC cc;
    tABC_Error error;
    unsigned char seed[] = {1, 2, 3};

    tABC_QuestionChoices *pChoices = NULL;

    if (argc != 2)
    {
        fprintf(stderr, "usage: %s <dir>\n", argv[0]);
        return 1;
    }

    MAIN_CHECK(ABC_Initialize(argv[1], NULL, 0, seed, sizeof(seed), &error));
    MAIN_CHECK(ABC_GetQuestionChoices(&pChoices, &error));

    printf("Choices:\n");
    for (unsigned i = 0; i < pChoices->numChoices; ++i)
    {
        printf(" %s (%s, %d)\n", pChoices->aChoices[i]->szQuestion,
                                  pChoices->aChoices[i]->szCategory,
                                  pChoices->aChoices[i]->minAnswerLength);
    }

    ABC_FreeQuestionChoices(pChoices);
    return 0;
}
