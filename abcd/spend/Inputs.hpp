/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_INPUTS_HPP
#define ABCD_BITCOIN_INPUTS_HPP

#include "../util/Status.hpp"
#include <bitcoin/bitcoin.hpp>
#include <map>
#include <unordered_map>

namespace abcd {

class TxCache;

/**
 * Maps from Bitcoin addresses to WIF-encoded private keys.
 */
typedef std::map<const std::string, std::string> KeyTable;

/**
 * Fills the transaction's inputs with signatures.
 */
Status
signTx(bc::transaction_type &result, const TxCache &txCache,
       const KeyTable &keys);

/**
 * Select a utxo collection that will satisfy the outputs as best possible
 * and calculate the resulting fees.
 */
Status
inputsPickOptimal(uint64_t &resultFee, uint64_t &resultChange,
                  bc::transaction_type &tx, bc::output_info_list &utxos);

/**
 * Populate the transaction's input list with all the utxo's in the wallet,
 * and calculate the mining fee using the already-present outputs.
 */
Status
inputsPickMaximum(uint64_t &resultFee, uint64_t &resultUsable,
                  bc::transaction_type &tx, bc::output_info_list &utxos);

} // namespace abcd

#endif

