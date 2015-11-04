/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_LOGIN_RECOVERY_QUESTIONS_HPP
#define ABCD_LOGIN_RECOVERY_QUESTIONS_HPP

#include "../util/Status.hpp"

namespace abcd {

void ABC_GeneralFreeQuestionChoices(tABC_QuestionChoices *pQuestionChoices);

tABC_CC ABC_GeneralGetQuestionChoices(tABC_QuestionChoices **ppQuestionChoices,
                                      tABC_Error *pError);

} // namespace abcd

#endif
