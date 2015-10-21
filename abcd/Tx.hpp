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
 * Functions for creating, viewing, and modifying transaction meta-data.
 */

#ifndef ABC_Tx_h
#define ABC_Tx_h

#include "util/Status.hpp"
#include <map>
#include <vector>

namespace abcd {

struct SendInfo;
struct TxMetadata;
class Wallet;

void ABC_TxFreeOutputs(tABC_TxOutput **aOutputs, unsigned int count);

tABC_CC ABC_TxSendComplete(Wallet &self,
                           SendInfo         *pInfo,
                           const std::string &ntxid,
                           const std::string &txid,
                           const std::vector<std::string> &addresses,
                           tABC_Error       *pError);

tABC_CC ABC_TxReceiveTransaction(Wallet &self,
                                 uint64_t amountSatoshi, uint64_t feeSatoshi,
                                 const std::string &ntxid,
                                 const std::string &txid,
                                 const std::vector<std::string> &addresses,
                                 tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                                 void *pData,
                                 tABC_Error *pError);

tABC_CC ABC_TxGetTransaction(Wallet &self,
                             const std::string &ntxid,
                             tABC_TxInfo **ppTransaction,
                             tABC_Error *pError);

tABC_CC ABC_TxGetTransactions(Wallet &self,
                              int64_t startTime,
                              int64_t endTime,
                              tABC_TxInfo ***paTransactions,
                              unsigned int *pCount,
                              tABC_Error *pError);

tABC_CC ABC_TxSearchTransactions(Wallet &self,
                                 const char *szQuery,
                                 tABC_TxInfo ***paTransactions,
                                 unsigned int *pCount,
                                 tABC_Error *pError);

void ABC_TxFreeTransaction(tABC_TxInfo *pTransactions);

void ABC_TxFreeTransactions(tABC_TxInfo **aTransactions,
                            unsigned int count);

tABC_CC ABC_TxSetTransactionDetails(Wallet &self,
                                    const std::string &ntxid,
                                    const TxMetadata &metadata,
                                    tABC_Error *pError);

tABC_CC ABC_TxGetTransactionDetails(Wallet &self,
                                    const std::string &ntxid,
                                    TxMetadata &result,
                                    tABC_Error *pError);

tABC_CC ABC_TxSweepSaveTransaction(Wallet &wallet,
                                   const std::string &ntxid,
                                   const std::string &txid,
                                   uint64_t funds,
                                   const TxMetadata &metadata,
                                   tABC_Error *pError);

} // namespace abcd

#endif
