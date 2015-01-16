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

#include "WalletAsync.hpp"
#include "../abcd/Wallet.hpp"
#include "../abcd/util/Util.hpp"

namespace abcd {

/**
 * Allocates the wallet create info structure and
 * populates it with the data given
 */
tABC_CC ABC_WalletCreateInfoAlloc(tABC_WalletCreateInfo **ppWalletCreateInfo,
                                  tABC_SyncKeys *pKeys,
                                  tABC_U08Buf L1,
                                  tABC_U08Buf LP1,
                                  const char *szUserName,
                                  const char *szWalletName,
                                  int        currencyNum,
                                  unsigned int attributes,
                                  tABC_Request_Callback fRequestCallback,
                                  void *pData,
                                  tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_WalletCreateInfo *pWalletCreateInfo;
    ABC_NEW(pWalletCreateInfo, tABC_WalletCreateInfo);

    ABC_CHECK_RET(ABC_SyncKeysCopy(&pWalletCreateInfo->pKeys, pKeys, pError));
    ABC_BUF_DUP(pWalletCreateInfo->L1, L1);
    ABC_BUF_DUP(pWalletCreateInfo->LP1, LP1);
    ABC_STRDUP(pWalletCreateInfo->szUserName, szUserName);

    ABC_STRDUP(pWalletCreateInfo->szWalletName, szWalletName);
    pWalletCreateInfo->currencyNum = currencyNum;
    pWalletCreateInfo->attributes = attributes;

    pWalletCreateInfo->fRequestCallback = fRequestCallback;

    pWalletCreateInfo->pData = pData;

    *ppWalletCreateInfo = pWalletCreateInfo;

exit:

    return cc;
}

/**
 * Frees the wallet creation info structure
 */
void ABC_WalletCreateInfoFree(tABC_WalletCreateInfo *pWalletCreateInfo)
{
    if (pWalletCreateInfo)
    {
        ABC_SyncFreeKeys(pWalletCreateInfo->pKeys);
        ABC_BUF_FREE(pWalletCreateInfo->L1);
        ABC_BUF_FREE(pWalletCreateInfo->LP1);
        ABC_FREE_STR(pWalletCreateInfo->szUserName);

        ABC_FREE_STR(pWalletCreateInfo->szWalletName);

        ABC_CLEAR_FREE(pWalletCreateInfo, sizeof(tABC_WalletCreateInfo));
    }
}

/**
 * Create a new wallet. Assumes it is running in a thread.
 *
 * This function creates a new wallet.
 * The function assumes it is in it's own thread (i.e., thread safe)
 * The callback will be called when it has finished.
 * The caller needs to handle potentially being in a seperate thread
 *
 * @param pData Structure holding all the data needed to create a wallet (should be a tABC_WalletCreateInfo)
 */
void *ABC_WalletCreateThreaded(void *pData)
{
    tABC_WalletCreateInfo *pInfo = (tABC_WalletCreateInfo *)pData;
    if (pInfo)
    {
        tABC_RequestResults results;
        memset(&results, 0, sizeof(tABC_RequestResults));

        results.requestType = ABC_RequestType_CreateWallet;

        results.bSuccess = false;

        // create the wallet
        tABC_CC CC = ABC_WalletCreate(pInfo->pKeys, pInfo->L1, pInfo->LP1, pInfo->szUserName,
            pInfo->szWalletName, pInfo->currencyNum, pInfo->attributes,
            (char **) &(results.pRetData), &(results.errorInfo));
        results.errorInfo.code = CC;

        // we are done so load up the info and ship it back to the caller via the callback
        results.pData = pInfo->pData;
        results.bSuccess = (CC == ABC_CC_Ok ? true : false);
        pInfo->fRequestCallback(&results);

        // it is our responsibility to free the info struct
        ABC_WalletCreateInfoFree(pInfo);
    }

    return NULL;
}

} // namespace abcd
