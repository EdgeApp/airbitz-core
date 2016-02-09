/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Caching and utility wrapper layer around the bitcoin `watcher` class.
 *
 * There was a time when `watcher` was part of libbitcoin-watcher,
 * and the AirBitz software was plain C.
 * This module used to bridge the gap between those two worlds,
 * but now it is less useful.
 */

#ifndef ABC_Bridge_h
#define ABC_Bridge_h

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include <functional>

namespace libbitcoin {
struct transaction_type;
}

namespace abcd {

typedef std::function<void(Status)> StatusCallback;
class Wallet;

Status
watcherDeleteCache(Wallet &self);

Status
watcherSave(Wallet &self);

std::string
watcherPath(Wallet &self);

tABC_CC ABC_BridgeSweepKey(Wallet &self,
                           tABC_U08Buf key,
                           bool compressed,
                           tABC_Error *pError);

tABC_CC ABC_BridgeWatcherStart(Wallet &self,
                               tABC_Error *pError);

tABC_CC ABC_BridgeWatcherLoop(Wallet &self,
                              tABC_BitCoin_Event_Callback fAsyncCallback,
                              void *pData,
                              tABC_Error *pError);

tABC_CC ABC_BridgeWatcherConnect(Wallet &self, tABC_Error *pError);

tABC_CC ABC_BridgeWatcherDisconnect(Wallet &self, tABC_Error *pError);

tABC_CC ABC_BridgeWatcherStop(Wallet &self, tABC_Error *pError);

tABC_CC ABC_BridgeWatcherDelete(Wallet &self, tABC_Error *pError);

Status
bridgeWatchAddress(const Wallet &self, const std::string &address);

tABC_CC ABC_BridgePrioritizeAddress(Wallet &self,
                                    const char *szAddress,
                                    tABC_Error *pError);

Status
watcherSend(Wallet &self, StatusCallback status, DataSlice rawTx);

tABC_CC ABC_BridgeFilterTransactions(Wallet &self,
                                     tABC_TxInfo **aTransactions,
                                     unsigned int *pCount,
                                     tABC_Error *pError);

} // namespace abcd

#endif
