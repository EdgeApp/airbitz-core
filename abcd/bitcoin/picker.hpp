/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */
#ifndef ABCD_BITCOIN_PICKER_HPP
#define ABCD_BITCOIN_PICKER_HPP

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/transaction.hpp>
#include "watcher.hpp"

namespace abcd {

enum {
    ok = 0,
    insufficent_funds,
    invalid_key,
    invalid_sig
};

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

}

#endif

