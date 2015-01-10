/**
 * @file
 * Storage backend for login data.
 */

#ifndef ABC_LoginDir_h
#define ABC_LoginDir_h

#include "ABC.h"
#include "LoginPackages.hpp"

namespace abcd {

    tABC_CC ABC_LoginDirGetNumber(const char *szUserName,
                                  int *pAccountNum,
                                  tABC_Error *pError);

    tABC_CC ABC_LoginDirCreate(int *pAccountNum,
                               const char *szUserName,
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

    tABC_CC ABC_LoginDirFileDelete(unsigned AccountNum,
                                   const char *szFile,
                                   tABC_Error *pError);

    tABC_CC ABC_LoginDirLoadPackages(int AccountNum,
                                     tABC_CarePackage **ppCarePackage,
                                     tABC_LoginPackage **ppLoginPackage,
                                     tABC_Error *pError);

    tABC_CC ABC_LoginDirSavePackages(int AccountNum,
                                     tABC_CarePackage *pCarePackage,
                                     tABC_LoginPackage *pLoginPackage,
                                     tABC_Error *pError);

    tABC_CC ABC_LoginDirGetSyncDir(int AccountNum,
                                   char **pszDirName,
                                   tABC_Error *pError);

    tABC_CC ABC_LoginDirMakeSyncDir(int AccountNum,
                                    char *szSyncKey,
                                    tABC_Error *pError);

} // namespace abcd

#endif
