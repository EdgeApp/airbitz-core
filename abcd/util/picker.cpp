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
    utx.code = ok;
    utx.tx.version = 1;
    utx.tx.locktime = 0;
    utx.tx.outputs = outputs;

    // Gather all the unspent outputs in the wallet:
    auto unspent = watcher.get_utxos(true);

    // Select a collection of outputs that satisfies our requirements:
    select_outputs_result utxos = select_outputs(unspent, amountSatoshi);
    if (utxos.points.size() <= 0)
    {
        utx.code = insufficent_funds;
        return false;
    }

    // Build the transaction's input list:
    for (auto &point : utxos.points)
    {
        transaction_input_type input;
        input.sequence = 4294967295;
        input.previous_output = point;
        utx.tx.inputs.push_back(input);
    }

    // If change is needed, add that to the output list:
    if (utxos.change > 0)
    {
        transaction_output_type change;
        change.value = utxos.change;
        change.script = build_pubkey_hash_script(changeAddr.hash());
        utx.tx.outputs.push_back(change);
    }
    return true;
}

BC_API bool sign_tx(unsigned_transaction_type& utx, std::vector<std::string>& keys, watcher& watcher)
{
    utx.code = ok;

    for (size_t i = 0; i < utx.tx.inputs.size(); ++i)
    {
        // Find the utxo this input refers to:
        bc::input_point& point = utx.tx.inputs[i].previous_output;
        bc::transaction_type tx = watcher.find_tx(point.hash);

        // Find the address for that utxo:
        bc::payment_address pa;
        bc::script_type& script = tx.outputs[point.index].script;
        bc::extract(pa, script);
        if (payment_address::invalid_version == pa.version())
        {
            utx.code = invalid_key;
            return false;
        }

        // Find the elliptic curve key for this input:
        bool found = false;
        bc::ec_secret secret;
        bc::ec_point pubkey;
        for (auto k : keys)
        {
            secret = bc::decode_hash(k);
            pubkey = bc::secret_to_public_key(secret, true);

            payment_address a;
            set_public_key(a, pubkey);
            if (a.encoded() == pa.encoded())
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            utx.code = invalid_key;
            return false;
        }

        // Gererate the previous output's signature:
        // TODO: We already have this; process it and use it
        script_type sig_script = build_pubkey_hash_script(pa.hash());

        // Generate the signature for this input:
        hash_digest sig_hash =
            script_type::generate_signature_hash(utx.tx, i, sig_script, 1);
        if (sig_hash == null_hash)
        {
            utx.code = invalid_sig;
            return false;
        }
        data_chunk signature = sign(secret, sig_hash,
            create_nonce(secret, sig_hash));
        signature.push_back(0x01);

        // Create out scriptsig:
        script_type scriptsig;
        scriptsig.push_operation(create_data_operation(signature));
        scriptsig.push_operation(create_data_operation(pubkey));
        utx.tx.inputs[i].script = scriptsig;
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

