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

struct TxInfo;
struct TxStatus;
class Wallet;

/**
 * Converts the modern `TxInfo` structure to the API's `tABC_TxInfo` structure,
 * using information from the wallet's metadatabase.
 */
tABC_TxInfo *
makeTxInfo(Wallet &self, const TxInfo &info, const TxStatus &status);

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

} // namespace abcd

#endif
