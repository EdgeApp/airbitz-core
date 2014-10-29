/**
 * @file
 * Password-based login logic.
 */

#ifndef ABC_LoginPassword_h
#define ABC_LoginPassword_h

#include "ABC.h"
#include "ABC_Login.h"

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_LoginPassword(tABC_Login **ppSelf,
                              const char *szUserName,
                              const char *szPassword,
                              tABC_Error *pError);

    tABC_CC ABC_LoginPasswordSet(tABC_Login *pSelf,
                                 const char *szPassword,
                                 tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
