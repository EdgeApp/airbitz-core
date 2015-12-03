/*
 * Copyright (c) 2014, AirBitz, Inc.
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

class Wallet;

/**
 * Maps from Bitcoin addresses to WIF-encoded private keys.
 */
typedef std::map<const std::string, std::string> KeyTable;

Status
signTx(bc::transaction_type &result, const Wallet &wallet,
       const KeyTable &keys);

/**
 * A fully-formed transaction, but possibly missing its signature scripts.
 * The challenges list contains the output challenge scripts,
 * one per input, that the signature scripts must solve.
 */
struct unsigned_transaction
{
    bc::transaction_type tx;
    std::vector<bc::script_type> challenges;
};

/**
 * A decoded WIF key.
 * This will move into libbitcoin/wallet at some point in the future.
 */
struct wif_key
{
    bc::ec_secret secret;
    bool compressed;
};

/**
 * A private key and its associated address.
 */
typedef std::unordered_map<bc::payment_address, wif_key> key_table;

/**
 * Finds the challenges for a set up utxos in the watcher database.
 */
bool gather_challenges(unsigned_transaction &utx, Wallet &wallet);

/**
 * Signs as many transaction inputs as possible using the given keys.
 * @return true if all inputs are now signed.
 */
bool sign_tx(unsigned_transaction &utx, const key_table &keys);

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
inputsPickMaximum(uint64_t &resultFee, uint64_t &resultChange,
                  bc::transaction_type &tx, bc::output_info_list &utxos);

} // namespace abcd

#endif

