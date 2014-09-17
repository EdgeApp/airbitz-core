/*
 * Copyright (c) 2011-2014 libbitcoin developers (see AUTHORS)
 *
 * This file is part of libbitcoin-watcher.
 *
 * libbitcoin-watcher is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef ABC_PICKER_HPP
#define ABC_PICKER_HPP

#include <bitcoin/bitcoin.hpp>
#include <bitcoin/transaction.hpp>
#include <bitcoin/watcher/watcher.hpp>

namespace picker {

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
             libwallet::watcher& watcher,
             const std::vector<bc::payment_address>& addresses,
             const bc::payment_address& changeAddr,
             int64_t amountSatoshi,
             fee_schedule& sched,
             bc::transaction_output_list& outputs,
             unsigned_transaction_type& utx);

BC_API bool sign_tx(unsigned_transaction_type& utx,
                    std::vector<std::string>& keys,
                    libwallet::watcher& watcher,
                    bc::ec_secret nonce);

}

#endif

