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

#include "Typedefs.hpp"
#include "../util/Data.hpp"

namespace abcd {

class Wallet;

Status
bridgeSweepKey(Wallet &self, const std::string &wif,
               const std::string &address);

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

} // namespace abcd

#endif
