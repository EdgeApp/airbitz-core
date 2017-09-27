/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Utility.hpp"
#include "../util/Data.hpp"

namespace abcd {

bc::hash_digest
makeNtxid(bc::transaction_type tx)
{
    for (auto &input: tx.inputs)
        input.script = bc::script_type();
    return bc::hash_transaction(tx, bc::sighash::all);
}

bool
isReplaceByFee(const bc::transaction_type &tx)
{
    for (const auto input: tx.inputs)
        if (input.sequence < 0xffffffff - 1)
            return true;
    return false;
}

bc::operation
makePushOperation(bc::data_slice data)
{
    BITCOIN_ASSERT(data.size() < std::numeric_limits<uint32_t>::max());
    bc::operation op;
    op.data = bc::data_chunk(data.begin(), data.end());
    if (!data.size())
        op.code = bc::opcode::zero;
    else if (data.size() <= 75)
        op.code = bc::opcode::special;
    else if (data.size() < std::numeric_limits<uint8_t>::max())
        op.code = bc::opcode::pushdata1;
    else if (data.size() < std::numeric_limits<uint16_t>::max())
        op.code = bc::opcode::pushdata2;
    else if (data.size() < std::numeric_limits<uint32_t>::max())
        op.code = bc::opcode::pushdata4;
    return op;
}

Status
decodeTx(bc::transaction_type &result, bc::data_slice rawTx)
{
    bc::transaction_type out;
    try
    {
        auto deserial = bc::make_deserializer(rawTx.begin(), rawTx.end());

        out.version = deserial.read_4_bytes();

        // Skip the next two bytes if this is segwit:
        bool isSegwit = false;
        if (deserial.iterator()[0] == 0x00 && deserial.iterator()[1] == 0x01)
        {
            isSegwit = true;
            deserial.read_2_bytes();
        }

        // Read inputs:
        uint64_t tx_in_count = deserial.read_variable_uint();
        for (size_t tx_in_i = 0; tx_in_i < tx_in_count; ++tx_in_i)
        {
            bc::transaction_input_type input;
            input.previous_output.hash = deserial.read_hash();
            input.previous_output.index = deserial.read_4_bytes();
            if (previous_output_is_null(input.previous_output))
                input.script = bc::raw_data_script(bc::read_raw_script(deserial));
            else
                input.script = bc::read_script(deserial);
            input.sequence = deserial.read_4_bytes();
            out.inputs.push_back(input);
        }

        // Read outputs:
        uint64_t tx_out_count = deserial.read_variable_uint();
        for (size_t tx_out_i = 0; tx_out_i < tx_out_count; ++tx_out_i)
        {
            bc::transaction_output_type output;
            output.value = deserial.read_8_bytes();
            output.script = bc::read_script(deserial);
            out.outputs.push_back(output);
        }

        // Read witnesses:
        if (isSegwit)
        {
            uint64_t witnessCount = deserial.read_variable_uint();
            for (size_t i = 0; i < witnessCount; ++i)
            {
                uint64_t witnessSize = deserial.read_variable_uint();
                deserial.read_data(witnessSize);
            }
        }

        // Read locktime:
        out.locktime = deserial.read_4_bytes();
    }
    catch (bc::end_of_stream)
    {
        return ABC_ERROR(ABC_CC_ParseError, "Bad transaction format - too little data");
    }

    result = std::move(out);
    return Status();
}

Status
decodeHeader(bc::block_header_type &result, bc::data_slice rawHeader)
{
    bc::block_header_type out;
    try
    {
        auto deserial = bc::make_deserializer(rawHeader.begin(), rawHeader.end());
        bc::satoshi_load(deserial.iterator(), deserial.end(), out);
    }
    catch (bc::end_of_stream)
    {
        return ABC_ERROR(ABC_CC_ParseError, "Bad transaction format");
    }

    result = std::move(out);
    return Status();
}

} // namespace abcd
