/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef SRC_TX_INFO_HPP
#define SRC_TX_INFO_HPP

#include "../abcd/util/Status.hpp"

namespace abcd {

struct TxMetadata;
class Wallet;

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

Status
bridgeFilterTransactions(Wallet &self,
                         tABC_TxInfo **aTransactions,
                         unsigned int *pCount);

} // namespace abcd

#endif
