/**
 * @file
 * Storage backend for login data.
 */

#ifndef ABC_LoginDir_h
#define ABC_LoginDir_h

#include "ABC.h"

#ifdef __cplusplus
extern "C" {
#endif

    #define ACCOUNT_SYNC_DIR                        "sync"
    #define ACCOUNT_NAME_FILENAME                   "UserName.json"

    // UserName.json:
    #define JSON_ACCT_USERNAME_FIELD                "userName"

    tABC_CC ABC_LoginDirExists(const char *szUserName,
                               tABC_Error *pError);

    tABC_CC ABC_LoginDirGetNumber(const char *szUserName,
                                  int *pAccountNum,
                                  tABC_Error *pError);

    tABC_CC ABC_LoginDirNewNumber(int *pAccountNum,
                                  tABC_Error *pError);

    tABC_CC ABC_LoginCreateSync(const char *szAccountsRootDir,
                                tABC_Error *pError);

    tABC_CC ABC_LoginCopyAccountDirName(char *szAccountDir,
                                        int AccountNum,
                                        tABC_Error *pError);

    tABC_CC ABC_LoginGetDirName(const char *szUserName,
                                char **pszDirName,
                                tABC_Error *pError);

    tABC_CC ABC_LoginGetSyncDirName(const char *szUserName,
                                    char **pszDirName,
                                    tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
