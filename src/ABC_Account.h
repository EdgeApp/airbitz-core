/**
 * @file
 * Functions for dealing with the contents of the account sync directory.
 */

#ifndef ABC_Account_h
#define ABC_Account_h

#include "ABC.h"
#include "ABC_Sync.h"

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_AccountCreate(tABC_SyncKeys *pKeys,
                              tABC_Error *pError);

    tABC_CC ABC_AccountCategoriesLoad(tABC_SyncKeys *pKeys,
                                      char ***paszCategories,
                                      unsigned int *pCount,
                                      tABC_Error *pError);

    tABC_CC ABC_AccountCategoriesAdd(tABC_SyncKeys *pKeys,
                                     char *szCategory,
                                     tABC_Error *pError);

    tABC_CC ABC_AccountCategoriesRemove(tABC_SyncKeys *pKeys,
                                        char *szCategory,
                                        tABC_Error *pError);

    tABC_CC ABC_AccountSettingsLoad(tABC_SyncKeys *pKeys,
                                    tABC_AccountSettings **ppSettings,
                                    tABC_Error *pError);

    tABC_CC ABC_AccountSettingsSave(tABC_SyncKeys *pKeys,
                                    tABC_AccountSettings *pSettings,
                                    tABC_Error *pError);

    void ABC_AccountSettingsFree(tABC_AccountSettings *pSettings);

#ifdef __cplusplus
}
#endif

#endif
