/**
 * @file
 * AirBitz Wallet function prototypes
 *
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 *
 *  @author See AUTHORS
 *  @version 1.0
 */

#ifndef ABC_Wallet_h
#define ABC_Wallet_h

#include "ABC.h"
#include "ABC_General.h"
#include "util/ABC_Util.h"

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_WalletRemoveFromCache(const char *szUUID, tABC_Error *pError);

    tABC_CC ABC_WalletClearCache(tABC_Error *pError);

    tABC_CC ABC_WalletSetName(const char *szUserName,
                              const char *szPassword,
                              const char *szUUID,
                              const char *szName,
                              tABC_Error *pError);

    tABC_CC ABC_WalletDirtyCache(const char *szUserName,
                                 const char *szPassword,
                                 const char *szUUID,
                                 tABC_Error *pError);

    tABC_CC ABC_WalletGetInfo(const char *szUserName,
                              const char *szPassword,
                              const char *szUUID,
                              tABC_WalletInfo **ppWalletInfo,
                              tABC_Error *pError);

    void ABC_WalletFreeInfo(tABC_WalletInfo *pWalletInfo);

    tABC_CC ABC_WalletGetWallets(const char *szUserName,
                                 const char *szPassword,
                                 tABC_WalletInfo ***paWalletInfo,
                                 unsigned int *pCount,
                                 tABC_Error *pError);

    void ABC_WalletFreeInfoArray(tABC_WalletInfo **aWalletInfo,
                                 unsigned int nCount);

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

    tABC_CC ABC_WalletGetBitcoinPrivateSeedDisk(const char *szUserName,
                                                const char *szPassword,
                                                const char *szUUID,
                                                tABC_U08Buf *pSeed,
                                                tABC_Error *pError);

    tABC_CC ABC_WalletGetDirName(char **pszDir,
                                 const char *szWalletUUID,
                                 tABC_Error *pError);

    tABC_CC ABC_WalletGetTxDirName(char **pszDir,
                                   const char *szWalletUUID,
                                   tABC_Error *pError);

    tABC_CC ABC_WalletGetAddressDirName(char **pszDir,
                                        const char *szWalletUUID,
                                        tABC_Error *pError);

    // Blocking functions:
    tABC_CC ABC_WalletCreate(const char *szUserName,
                             const char *szPassword,
                             const char *szWalletName,
                             int  currencyNum,
                             unsigned int attributes,
                             char **pszUUID,
                             tABC_Error *pError);

    tABC_CC ABC_WalletSyncAll(const char *szUserName,
                              const char *szPassword,
                              int *pDirty,
                              tABC_Error *pError);

    tABC_CC ABC_WalletSyncData(const char *szUserName,
                               const char *szPassword,
                               const char *szUUID,
                               int *pDirty,
                               tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
