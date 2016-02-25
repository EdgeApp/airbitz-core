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
class Wallet;

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

} // namespace abcd

#endif
