/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
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

/**
 * Saves a transaction to the txdb after sweeping.
 */
Status
txSweepSave(Wallet &self,
            const std::string &ntxid, const std::string &txid,
            uint64_t funds);

/**
 * Saves a transaction to the txdb after sending.
 */
Status
txSendSave(Wallet &self,
           const std::string &ntxid, const std::string &txid,
           const std::vector<std::string> &addresses, SendInfo *pInfo);

/**
 * Handles creating or updating when we receive a transaction.
 */
Status
txReceiveTransaction(Wallet &self,
                     const std::string &ntxid, const std::string &txid,
                     const std::vector<std::string> &addresses,
                     tABC_BitCoin_Event_Callback fAsyncCallback, void *pData);

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

} // namespace abcd

#endif
