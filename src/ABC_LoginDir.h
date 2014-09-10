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

    #define ACCOUNT_CARE_PACKAGE_FILENAME           "CarePackage.json"
    #define ACCOUNT_LOGIN_PACKAGE_FILENAME          "LoginPackage.json"

    tABC_CC ABC_LoginDirExists(const char *szUserName,
                               tABC_Error *pError);

    tABC_CC ABC_LoginDirGetNumber(const char *szUserName,
                                  int *pAccountNum,
                                  tABC_Error *pError);

    tABC_CC ABC_LoginDirCreate(const char *szUserName,
                               const char *szCarePackageJSON,
                               const char *szLoginPackageJSON,
                               tABC_Error *pError);

    tABC_CC ABC_LoginGetSyncDirName(const char *szUserName,
                                    char **pszDirName,
                                    tABC_Error *pError);

    tABC_CC ABC_LoginDirFileLoad(char **pszData,
                                 unsigned AccountNum,
                                 const char *szFile,
                                 tABC_Error *pError);

    tABC_CC ABC_LoginDirFileSave(const char *szData,
                                 unsigned AccountNum,
                                 const char *szFile,
                                 tABC_Error *pError);

    tABC_CC ABC_LoginDirFileExists(bool *pbExists,
                                   unsigned AccountNum,
                                   const char *szFile,
                                   tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
