/**
 * @file
 * AirBitz SQL server API.
 */

#ifndef ABC_LoginServer_h
#define ABC_LoginServer_h

#include "../src/ABC.h"
#include "LoginPackages.hpp"
#include "util/Sync.hpp"
#include <time.h>

namespace abcd {

    tABC_CC ABC_LoginServerCreate(tABC_U08Buf L1,
                                  tABC_U08Buf LP1,
                                  tABC_CarePackage *pCarePackage,
                                  tABC_LoginPackage *pLoginPackage,
                                  char *szRepoAcctKey,
                                  tABC_Error *pError);

    tABC_CC ABC_LoginServerActivate(tABC_U08Buf L1,
                                    tABC_U08Buf LP1,
                                    tABC_Error *pError);

    tABC_CC ABC_LoginServerChangePassword(tABC_U08Buf L1,
                                          tABC_U08Buf oldLP1,
                                          tABC_U08Buf newLP1,
                                          tABC_U08Buf newLRA1,
                                          tABC_CarePackage *pCarePackage,
                                          tABC_LoginPackage *pLoginPackage,
                                          tABC_Error *pError);

    tABC_CC ABC_LoginServerGetCarePackage(tABC_U08Buf L1,
                                          tABC_CarePackage **ppCarePackage,
                                          tABC_Error *pError);

    tABC_CC ABC_LoginServerGetLoginPackage(tABC_U08Buf L1,
                                           tABC_U08Buf LP1,
                                           tABC_U08Buf LRA1,
                                           tABC_LoginPackage **ppLoginPackage,
                                           tABC_Error *pError);

    tABC_CC ABC_LoginServerGetPinPackage(tABC_U08Buf DID,
                                         tABC_U08Buf LPIN1,
                                         char **szPinPackage,
                                         tABC_Error *pError);

    tABC_CC ABC_LoginServerUpdatePinPackage(tABC_U08Buf L1,
                                            tABC_U08Buf LP1,
                                            tABC_U08Buf DID,
                                            tABC_U08Buf LPIN1,
                                            char *szPinPackage,
                                            time_t ali,
                                            tABC_Error *pError);

    tABC_CC ABC_WalletServerRepoPost(tABC_U08Buf L1,
                                     tABC_U08Buf LP1,
                                     const char *szWalletAcctKey,
                                     const char *szPath,
                                     tABC_Error *pError);

    tABC_CC ABC_LoginServerUploadLogs(tABC_U08Buf L1,
                                      tABC_U08Buf LP1,
                                      tABC_SyncKeys *pKeys,
                                      tABC_Error *pError);

} // namespace abcd

#endif
