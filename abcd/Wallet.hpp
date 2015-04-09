/*
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
 */
/**
 * @file
 * Functions for dealing with the contents of the wallet sync directory.
 */

#ifndef ABC_Wallet_h
#define ABC_Wallet_h

#include "util/Data.hpp"
#include "../src/ABC.h"

namespace abcd {

class Account;
class Wallet;

typedef Wallet &tABC_WalletID;

std::string walletDir(const std::string &id);
std::string walletSyncDir(const std::string &id);
std::string walletAddressDir(const std::string &id);
std::string walletTxDir(const std::string &id);

void ABC_WalletClearCache();

tABC_CC ABC_WalletSetName(tABC_WalletID self,
                          const char *szName,
                          tABC_Error *pError);

tABC_CC ABC_WalletDirtyCache(tABC_WalletID self,
                             tABC_Error *pError);

tABC_CC ABC_WalletGetInfo(tABC_WalletID self,
                          tABC_WalletInfo **ppWalletInfo,
                          tABC_Error *pError);

void ABC_WalletFreeInfo(tABC_WalletInfo *pWalletInfo);

tABC_CC ABC_WalletGetMK(tABC_WalletID self,
                        tABC_U08Buf *pMK,
                        tABC_Error *pError);

tABC_CC ABC_WalletGetBitcoinPrivateSeed(tABC_WalletID self,
                                        tABC_U08Buf *pSeed,
                                        tABC_Error *pError);

tABC_CC ABC_WalletGetBitcoinPrivateSeedDisk(tABC_WalletID self,
                                            tABC_U08Buf *pSeed,
                                            tABC_Error *pError);

// Blocking functions:
tABC_CC ABC_WalletCreate(Account &account,
                         const char *szWalletName,
                         int  currencyNum,
                         char **pszUUID,
                         tABC_Error *pError);

tABC_CC ABC_WalletSyncData(tABC_WalletID self,
                           bool &dirty,
                           tABC_Error *pError);

} // namespace abcd

#endif
