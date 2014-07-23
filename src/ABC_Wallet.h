/**
 * @file
 * AirBitz Wallet function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_Wallet_h
#define ABC_Wallet_h

#include "ABC.h"
#include "ABC_General.h"
#include "ABC_Util.h"

#ifdef __cplusplus
extern "C" {
#endif

    /**
     * AirBitz Core Create Wallet Structure
     *
     * This structure contains the detailed information associated
     * with creating a new wallet.
     *
     */
    typedef struct sABC_WalletCreateInfo
    {
        /** data pointer given by caller at initial create call time */
        void                    *pData;

        char                    *szUserName;
        char                    *szPassword;
        char                    *szWalletName;
        int                     currencyNum;
        unsigned int            attributes;
        tABC_Request_Callback   fRequestCallback;
    } tABC_WalletCreateInfo;


    tABC_CC ABC_WalletCreateInfoAlloc(tABC_WalletCreateInfo **ppWalletCreateInfo,
                                      const char *szUserName,
                                      const char *szPassword,
                                      const char *szWalletName,
                                      int        currencyNum,
                                      unsigned int  attributes,
                                      tABC_Request_Callback fRequestCallback,
                                      void *pData,
                                      tABC_Error *pError);

    void ABC_WalletCreateInfoFree(tABC_WalletCreateInfo *pWalletCreateInfo);

    void * ABC_WalletCreateThreaded(void *pData);

    tABC_CC ABC_WalletClearCache(tABC_Error *pError);

    tABC_CC ABC_WalletSetName(const char *szUserName,
                              const char *szPassword,
                              const char *szUUID,
                              const char *szName,
                              tABC_Error *pError);

    tABC_CC ABC_WalletSetAttributes(const char *szUserName,
                                    const char *szPassword,
                                    const char *szUUID,
                                    unsigned int attributes,
                                    tABC_Error *pError);

    tABC_CC ABC_WalletGetInfo(const char *szUserName,
                              const char *szPassword,
                              const char *szUUID,
                              tABC_WalletInfo **ppWalletInfo,
                              tABC_Error *pError);

    void ABC_WalletFreeInfo(tABC_WalletInfo *pWalletInfo);

    tABC_CC ABC_WalletGetUUIDs(const char *szUserName,
                               char ***paUUIDs,
                               unsigned int *pCount,
                               tABC_Error *pError);

    tABC_CC ABC_WalletGetWallets(const char *szUserName,
                                 const char *szPassword,
                                 tABC_WalletInfo ***paWalletInfo,
                                 unsigned int *pCount,
                                 tABC_Error *pError);

    void ABC_WalletFreeInfoArray(tABC_WalletInfo **aWalletInfo,
                                 unsigned int nCount);

    tABC_CC ABC_WalletSetOrder(const char *szUserName,
                               const char *szPassword,
                               char **aszUUIDArray,
                               unsigned int countUUIDs,
                               tABC_Error *pError);

    tABC_CC ABC_WalletChangeEMKsForAccount(const char *szUserName,
                                           tABC_U08Buf oldLP2,
                                           tABC_U08Buf newLP2,
                                           tABC_Error *pError);

    tABC_CC ABC_WalletGetMK(const char *szUserName,
                            const char *szPassword,
                            const char *szUUID,
                            tABC_U08Buf *pMK,
                            tABC_Error *pError);

    tABC_CC ABC_WalletGetBitcoinPrivateSeed(const char *szUserName,
                                            const char *szPassword,
                                            const char *szUUID,
                                            tABC_U08Buf *pSeed,
                                            tABC_Error *pError);

    tABC_CC ABC_WalletGetTxDirName(char **pszDir,
                                   const char *szWalletUUID,
                                   tABC_Error *pError);

    tABC_CC ABC_WalletGetAddressDirName(char **pszDir,
                                        const char *szWalletUUID,
                                        tABC_Error *pError);

    tABC_CC ABC_WalletCheckCredentials(const char *szUserName,
                                       const char *szPassword,
                                       const char *szUUID,
                                       tABC_Error *pError);

    // Blocking functions:
    tABC_CC ABC_WalletCreate(tABC_WalletCreateInfo *pInfo,
                             char **pszUUID,
                             tABC_Error *pError);

    tABC_CC ABC_WalletSyncAll(const char *szUserName,
                              const char *szPassword,
                              int *pDirty,
                              tABC_Error *pError);

    tABC_CC ABC_WalletSyncData(const char *szUserName,
                               const char *szPassword,
                               const char *szUUID,
                               tABC_GeneralInfo *pInfo,
                               int *pDirty,
                               tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
