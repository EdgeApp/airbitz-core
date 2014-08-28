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
#include <unistd.h>
#include <iostream>
#include <bitcoin/bitcoin.hpp>
#include <wallet/transaction.hpp>

#include "picker.hpp"

namespace picker {

using namespace libbitcoin;
using namespace libwallet;

static std::map<data_chunk, std::string> address_map;
static script_type build_pubkey_hash_script(const short_hash& pubkey_hash);
static operation create_data_operation(data_chunk& data);

BC_API bool make_tx(
             watcher& watcher,
             const std::vector<payment_address>& addresses,
             const payment_address& changeAddr,
             int64_t amountSatoshi,
             fee_schedule& sched,
             transaction_output_list& outputs,
             unsigned_transaction_type& utx)
{
    output_info_list unspent;
    utx.code = ok;
    for (auto pa : addresses)
    {
        for (auto npa : watcher.get_utxos(pa))
        {
            hash_digest h = npa.point.hash;
            utx.output_map[h] = pa;
            unspent.push_back(npa);
        }
    }
    select_outputs_result os = select_outputs(unspent, amountSatoshi);
    /* Do we have the funds ? */
    if (os.points.size() <= 0)
    {
        utx.code = insufficent_funds;
        return false;
    }

    utx.tx.version = 1;
    utx.tx.locktime = 0;
    auto it = os.points.begin();
    for (; it != os.points.end(); it++)
    {
        transaction_input_type input;
        input.sequence = 4294967295;
        input.previous_output.index = it->index;
        input.previous_output.hash = it->hash;
        utx.tx.inputs.push_back(input);
    }
    utx.tx.outputs = outputs;
    /* If change is needed, add the change address */
    if (os.change > 0)
    {
        transaction_output_type change;
        change.value = os.change;
        change.script = build_pubkey_hash_script(changeAddr.hash());
        utx.tx.outputs.push_back(change);
    }
    /* Calculate fees with this transaction */
    utx.fees = sched.satoshi_per_kb * (satoshi_raw_size(utx.tx) / 1024);
    return true;
}

BC_API bool sign_tx(unsigned_transaction_type& utx, std::vector<elliptic_curve_key>& keys)
{
    std::map<data_chunk, elliptic_curve_key> key_map;
    for (size_t i = 0; i < utx.tx.inputs.size(); ++i)
    {
        utx.code = ok;
        transaction_input_type input = utx.tx.inputs[i];
        hash_digest pubHash = input.previous_output.hash;

        auto pa = utx.output_map.find(pubHash);
        if (pa == utx.output_map.end())
        {
            utx.code = invalid_key;
            return false;
        }
        elliptic_curve_key key;
        /* Find an elliptic_curve_key for this input */
        for (auto k : keys)
        {
            payment_address a;
            set_public_key(a, k.public_key());

            if (a.encoded() == pa->second.encoded())
            {
                key.set_secret(k.secret());
                break;
            }
        }
        data_chunk public_key = key.public_key();
        if (public_key.empty())
        {
            utx.code = invalid_key;
            return false;
        }
        /* Create the input script */
        script_type sig_script = build_pubkey_hash_script(pa->second.hash());

        hash_digest sig_hash =
            script_type::generate_signature_hash(utx.tx, i, sig_script, 1);
        if (sig_hash == null_hash)
        {
            utx.code = invalid_sig;
            return false;
        }
        data_chunk signature = key.sign(sig_hash);
        signature.push_back(0x01);

        script_type new_script_object;
        operation opsig = create_data_operation(signature);
        new_script_object.push_operation(opsig);

        operation opkey = create_data_operation(public_key);
        new_script_object.push_operation(opkey);

        utx.tx.inputs[i].script = new_script_object;
    }
    return true;
}

static script_type build_pubkey_hash_script(const short_hash& pubkey_hash)
{
    script_type result;
    result.push_operation({opcode::dup, data_chunk()});
    result.push_operation({opcode::hash160, data_chunk()});
    result.push_operation({opcode::special,
            data_chunk(pubkey_hash.begin(), pubkey_hash.end())});
    result.push_operation({opcode::equalverify, data_chunk()});
    result.push_operation({opcode::checksig, data_chunk()});
    return result;
}

static operation create_data_operation(data_chunk& data)
{
    BITCOIN_ASSERT(data.size() < std::numeric_limits<uint32_t>::max());
    operation op;
    op.data = data;
    if (data.size() <= 75)
        op.code = opcode::special;
    else if (data.size() < std::numeric_limits<uint8_t>::max())
        op.code = opcode::pushdata1;
    else if (data.size() < std::numeric_limits<uint16_t>::max())
        op.code = opcode::pushdata2;
    else if (data.size() < std::numeric_limits<uint32_t>::max())
        op.code = opcode::pushdata4;
    return op;
}

}

