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

#ifndef ABC_WalletAsync_h
#define ABC_WalletAsync_h

#include "ABC.h"

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

    void *ABC_WalletCreateThreaded(void *pData);

#ifdef __cplusplus
}
#endif

#endif
