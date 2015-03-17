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
 * Functions for dealing with the contents of the account sync directory.
 */

#ifndef ABC_Account_h
#define ABC_Account_h

#include "../util/Sync.hpp"

namespace abcd {

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

void ABC_AccountWalletInfoFree(tABC_AccountWalletInfo *pInfo);

tABC_CC ABC_AccountWalletList(tABC_SyncKeys *pKeys,
                              char ***paszUUID,
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
                                 const char *szUUIDs,
                                 tABC_Error *pError);

struct AutoAccountWalletInfo:
    public tABC_AccountWalletInfo
{
    ~AutoAccountWalletInfo()
    {
        ABC_AccountWalletInfoFree(this);
    }

    AutoAccountWalletInfo()
    {
        memset(this, 0, sizeof(*this));
    }
};

} // namespace abcd

#endif
