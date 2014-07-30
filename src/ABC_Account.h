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

    /**
     * Account-level wallet structure.
     *
     * This structure contains the information stored for a wallet at thee
     * account level.
     */
    typedef struct sABC_AccountWalletInfo
    {
        /** Unique wallet id. */
        char *szUUID;
        /** Bitcoin master seed. */
        tABC_U08Buf BitcoinSeed;
        /** The sync key used to access the server. */
        tABC_U08Buf SyncKey;
        /** The encryption key used to protect the contents. */
        tABC_U08Buf MK;
        /** Sort order. */
        unsigned sortIndex;
        /** True if the wallet should be hidden. */
        bool archived;
    } tABC_AccountWalletInfo;

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

    void ABC_AccountWalletInfoFree(tABC_AccountWalletInfo *pInfo);
    void ABC_AccountWalletInfoFreeArray(tABC_AccountWalletInfo *aInfo,
                                        unsigned count);

    tABC_CC ABC_AccountWalletList(tABC_SyncKeys *pKeys,
                                  char ***paszUUID,
                                  unsigned *pCount,
                                  tABC_Error *pError);

    tABC_CC ABC_AccountWalletsLoad(tABC_SyncKeys *pKeys,
                                   tABC_AccountWalletInfo **paInfo,
                                   unsigned *pCount,
                                   tABC_Error *pError);

    tABC_CC ABC_AccountWalletLoad(tABC_SyncKeys *pKeys,
                                  const char *szUUID,
                                  tABC_AccountWalletInfo *pInfo,
                                  tABC_Error *pError);

    tABC_CC ABC_AccountWalletSave(tABC_SyncKeys *pKeys,
                                  tABC_AccountWalletInfo *pInfo,
                                  tABC_Error *pError);

    tABC_CC ABC_AccountWalletReorder(tABC_SyncKeys *pKeys,
                                     char **aszUUID,
                                     unsigned count,
                                     tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
