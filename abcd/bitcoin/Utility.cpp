/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Utility.hpp"

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
        bc::satoshi_load(deserial.iterator(), deserial.end(), out);
    }
    catch (bc::end_of_stream)
    {
        return ABC_ERROR(ABC_CC_ParseError, "Bad transaction format");
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
