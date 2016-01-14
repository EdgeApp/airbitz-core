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

Status
bridgeSweepKey(Wallet &self, DataSlice key, bool compressed);

Status
bridgeWatcherStart(Wallet &self);

Status
bridgeWatcherLoop(Wallet &self,
                  tABC_BitCoin_Event_Callback fCallback,
                  void *pData);

Status
bridgeWatcherConnect(Wallet &self);

Status
bridgeWatcherDisconnect(Wallet &self);

Status
bridgeWatcherStop(Wallet &self);

Status
bridgeWatcherDelete(Wallet &self);

Status
watcherSend(Wallet &self, StatusCallback status, DataSlice rawTx);

Status
bridgeFilterTransactions(Wallet &self,
                         tABC_TxInfo **aTransactions,
                         unsigned int *pCount);

} // namespace abcd

#endif
