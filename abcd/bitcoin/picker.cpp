/*
 *  Copyright (c) 2014, AirBitz, Inc.
 *  All rights reserved.
 */
#include "picker.hpp"

#include <unistd.h>
#include <iostream>
#include <bitcoin/bitcoin.hpp>
#include <wallet/transaction.hpp>

namespace abcd {

using namespace libbitcoin;
using namespace libwallet;

static std::map<data_chunk, std::string> address_map;
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

script_type build_pubkey_hash_script(const short_hash& pubkey_hash)
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

bool gather_challenges(unsigned_transaction& utx, abcd::watcher& watcher)
{
    utx.challenges.resize(utx.tx.inputs.size());

    for (size_t i = 0; i < utx.tx.inputs.size(); ++i)
    {
        bc::input_point& point = utx.tx.inputs[i].previous_output;
        if (!watcher.db().has_tx(point.hash))
            return false;
        bc::transaction_type tx = watcher.find_tx(point.hash);
        utx.challenges[i] = tx.outputs[point.index].script;
    }

    return true;
}

bool sign_tx(unsigned_transaction& utx, const key_table& keys)
{
    bool all_done = true;

    for (size_t i = 0; i < utx.tx.inputs.size(); ++i)
    {
        auto& input = utx.tx.inputs[i];
        auto& challenge = utx.challenges[i];

        // Already signed?
        if (input.script.operations().size())
            continue;

        // Extract the address:
        bc::payment_address from_address;
        if (!bc::extract(from_address, challenge))
        {
            all_done = false;
            continue;
        }

        // Find a matching key:
        auto key = keys.find(from_address);
        if (key == keys.end())
        {
            all_done = false;
            continue;
        }
        auto& secret = key->second.secret;
        auto pubkey = bc::secret_to_public_key(secret, key->second.compressed);

        // Create the sighash for this input:
        hash_digest sighash =
            script_type::generate_signature_hash(utx.tx, i, challenge, 1);
        if (sighash == null_hash)
        {
            all_done = false;
            continue;
        }

        // Sign:
        data_chunk signature = sign(secret, sighash,
            create_nonce(secret, sighash));
        signature.push_back(0x01);

        // Save:
        script_type scriptsig;
        scriptsig.push_operation(create_data_operation(signature));
        scriptsig.push_operation(create_data_operation(pubkey));
        utx.tx.inputs[i].script = scriptsig;
    }

    return all_done;
}

}

