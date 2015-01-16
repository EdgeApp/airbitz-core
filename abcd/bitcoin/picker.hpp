/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_BITCOIN_PICKER_HPP
#define ABCD_BITCOIN_PICKER_HPP

#include "watcher.hpp"
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/transaction.hpp>

namespace abcd {

enum {
    ok = 0,
    insufficent_funds,
    invalid_key,
    invalid_sig
};

// This will go away:
struct unsigned_transaction_type
{
    bc::transaction_type tx;
    int code;
};

struct fee_schedule
{
    uint64_t satoshi_per_kb;
};

BC_API bool make_tx(
             abcd::watcher& watcher,
             const std::vector<bc::payment_address>& addresses,
             const bc::payment_address& changeAddr,
             int64_t amountSatoshi,
             fee_schedule& sched,
             bc::transaction_output_list& outputs,
             unsigned_transaction_type& utx);

BC_API bool sign_tx(unsigned_transaction_type& utx,
                    std::vector<std::string>& keys,
                    abcd::watcher& watcher);

bc::script_type build_pubkey_hash_script(const bc::short_hash& pubkey_hash);

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
bool gather_challenges(unsigned_transaction& utx, abcd::watcher& watcher);

/**
 * Signs as many transaction inputs as possible using the given keys.
 * @return true if all inputs are now signed.
 */
bool sign_tx(unsigned_transaction& utx, const key_table& keys);

} // namespace abcd

#endif

