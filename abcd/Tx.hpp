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
#include <vector>

namespace abcd {

class Wallet;

/**
 * Saves a transaction to the metadatabase after sweeping.
 */
Status
txSweepSave(Wallet &self,
            const std::string &ntxid, const std::string &txid,
            uint64_t funds);

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
