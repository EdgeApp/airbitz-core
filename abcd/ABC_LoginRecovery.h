/**
 * @file
 * Recovery-question login logic.
 */

#ifndef ABC_LoginRecovery_h
#define ABC_LoginRecovery_h

#include "ABC.h"
#include "ABC_Login.h"

namespace abcd {

    tABC_CC ABC_LoginGetRQ(const char *szUserName,
                           char **pszRecoveryQuestions,
                           tABC_Error *pError);

    tABC_CC ABC_LoginRecovery(tABC_Login **ppSelf,
                              const char *szUserName,
                              const char *szRecoveryAnswers,
                              tABC_Error *pError);

    tABC_CC ABC_LoginRecoverySet(tABC_Login *pSelf,
                                 const char *szRecoveryQuestions,
                                 const char *szRecoveryAnswers,
                                 tABC_Error *pError);

} // namespace abcd

#endif
