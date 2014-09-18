/**
 * @file
 * AirBitz SQL server API.
 */

#ifndef ABC_LoginServer_h
#define ABC_LoginServer_h

#include "ABC.h"
#include "util/ABC_Sync.h"

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_LoginServerCreate(tABC_U08Buf L1,
                                  tABC_U08Buf LP1,
                                  const char *szCarePackage_JSON,
                                  const char *szLoginPackage_JSON,
                                  char *szRepoAcctKey,
                                  tABC_Error *pError);

    tABC_CC ABC_LoginServerActivate(tABC_U08Buf L1,
                                    tABC_U08Buf LP1,
                                    tABC_Error *pError);

    tABC_CC ABC_LoginServerChangePassword(tABC_U08Buf L1,
                                          tABC_U08Buf oldLP1,
                                          tABC_U08Buf oldLRA1,
                                          tABC_U08Buf newLP1,
                                          tABC_U08Buf newLRA1,
                                          const char *szCarePackage,
                                          const char *szLoginPackage,
                                          tABC_Error *pError);

    tABC_CC ABC_LoginServerGetCarePackage(tABC_U08Buf L1,
                                          char **szResponse,
                                          tABC_Error *pError);

    tABC_CC ABC_LoginServerGetLoginPackage(tABC_U08Buf L1,
                                           tABC_U08Buf LP1,
                                           tABC_U08Buf LRA1,
                                           char **szLoginPackage,
                                           tABC_Error *pError);

    tABC_CC ABC_LoginServerUploadLogs(const char *szUserName,
                                      const char *szPassword,
                                      tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
