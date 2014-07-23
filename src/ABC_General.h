/**
 * @file
 * AirBitz general, non-account-specific server-supplied data.
 */

#ifndef ABC_General_h
#define ABC_General_h

#include "ABC.h"

#ifdef __cplusplus
extern "C" {
#endif

    void ABC_GeneralFreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices);

    tABC_CC ABC_GeneralGetQuestionChoices(tABC_QuestionChoices **ppQuestionChoices,
                                          tABC_Error *pError);

    tABC_CC ABC_GeneralUpdateQuestionChoices(tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
