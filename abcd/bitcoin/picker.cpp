/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "picker.hpp"
#include "Watcher.hpp"
#include <unistd.h>
#include <wallet/transaction.hpp>

namespace abcd {

using namespace libbitcoin;
using namespace libwallet;

constexpr unsigned min_output = 5430;

static std::map<data_chunk, std::string> address_map;
static operation create_data_operation(data_chunk& data);

Status
makeTx(bc::transaction_type &result, Watcher &watcher,
    const bc::payment_address &changeAddr,
    int64_t amountSatoshi,
    bc::transaction_output_list &outputs)
{
    bc::transaction_type out;

    out.version = 1;
    out.locktime = 0;
    out.outputs = outputs;

    // Gather all the unspent outputs in the wallet:
    auto unspent = watcher.get_utxos(true);

    // Select a collection of outputs that satisfies our requirements:
    select_outputs_result utxos = select_outputs(unspent, amountSatoshi);
    if (utxos.points.size() <= 0)
        return ABC_ERROR(ABC_CC_InsufficientFunds, "Insufficent funds");

    // Build the transaction's input list:
    for (auto &point : utxos.points)
    {
        transaction_input_type input;
        input.sequence = 4294967295;
        input.previous_output = point;
        out.inputs.push_back(input);
    }

    // If change is needed, add that to the output list:
    if (utxos.change > 0)
    {
        transaction_output_type change;
        change.value = utxos.change;
        change.script = build_pubkey_hash_script(changeAddr.hash());
        out.outputs.push_back(change);
    }

    // Remove any dust outputs, returning those funds to the miners:
    auto last = std::remove_if(out.outputs.begin(), out.outputs.end(),
        [](transaction_output_type &o){ return o.value < min_output; });
    out.outputs.erase(last, out.outputs.end());

    // If all the outputs were dust, we can't send this transaction:
    if (!out.outputs.size())
        return ABC_ERROR(ABC_CC_InsufficientFunds, "No remaining outputs");

    result = std::move(out);
    return Status();
}

Status
signTx(bc::transaction_type &result, Watcher &watcher,
    std::vector<std::string> &keys)
{
    for (size_t i = 0; i < result.inputs.size(); ++i)
    {
        // Find the utxo this input refers to:
        bc::input_point& point = result.inputs[i].previous_output;
        bc::transaction_type tx = watcher.find_tx(point.hash);

        // Find the address for that utxo:
        bc::payment_address pa;
        bc::script_type& script = tx.outputs[point.index].script;
        bc::extract(pa, script);
        if (payment_address::invalid_version == pa.version())
            return ABC_ERROR(ABC_CC_Error, "Invalid address");

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
            return ABC_ERROR(ABC_CC_Error, "Missing signing key");

        // Gererate the previous output's signature:
        // TODO: We already have this; process it and use it
        script_type sig_script = build_pubkey_hash_script(pa.hash());

        // Generate the signature for this input:
        hash_digest sig_hash =
            script_type::generate_signature_hash(result, i, sig_script, 1);
        if (sig_hash == null_hash)
            return ABC_ERROR(ABC_CC_Error, "Unable to sign");
        data_chunk signature = sign(secret, sig_hash,
            create_nonce(secret, sig_hash));
        signature.push_back(0x01);

        // Create out scriptsig:
        script_type scriptsig;
        scriptsig.push_operation(create_data_operation(signature));
        scriptsig.push_operation(create_data_operation(pubkey));
        result.inputs[i].script = scriptsig;
    }

    return Status();
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

bool gather_challenges(unsigned_transaction& utx, Watcher& watcher)
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

} // namespace abcd
