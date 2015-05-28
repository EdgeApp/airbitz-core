/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "Outputs.hpp"
#include "Spend.hpp"
#include "../bitcoin/Testnet.hpp"

namespace abcd {

bc::script_type
outputScriptForPubkey(const bc::short_hash &hash)
{
    bc::script_type result;
    result.push_operation({bc::opcode::dup, bc::data_chunk()});
    result.push_operation({bc::opcode::hash160, bc::data_chunk()});
    result.push_operation({bc::opcode::special, bc::data_chunk(hash.begin(), hash.end())});
    result.push_operation({bc::opcode::equalverify, bc::data_chunk()});
    result.push_operation({bc::opcode::checksig, bc::data_chunk()});
    return result;
}

static bc::script_type
outputScriptForScript(const bc::short_hash &hash)
{
    bc::script_type result;
    result.push_operation({bc::opcode::hash160, bc::data_chunk()});
    result.push_operation({bc::opcode::special, bc::data_chunk(hash.begin(), hash.end())});
    result.push_operation({bc::opcode::equal, bc::data_chunk()});
    return result;
}

Status
outputScriptForAddress(bc::script_type &result, const std::string &address)
{
    bc::payment_address parsed;
    if (!parsed.set_encoded(address))
        return ABC_ERROR(ABC_CC_ParseError, "Bad address " + address);

    if (parsed.version() == pubkeyVersion())
        result = outputScriptForPubkey(parsed.hash());
    else if (parsed.version() == scriptVersion())
        result = outputScriptForScript(parsed.hash());
    else
        return ABC_ERROR(ABC_CC_ParseError, "Non-Bitcoin address " + address);

    return Status();
}

Status
outputsForSendInfo(bc::transaction_output_list &result, sABC_TxSendInfo *pInfo)
{
    bc::transaction_output_list out;

    // The output being requested:
    bc::transaction_output_type output;
    output.value = pInfo->pDetails->amountSatoshi;
    ABC_CHECK(outputScriptForAddress(output.script, pInfo->szDestAddress));
    out.push_back(output);

    // Handle the Airbitz fees:
    pInfo->pDetails->amountFeesAirbitzSatoshi = 0;
    if (!pInfo->bTransfer)
    {
        // We would normally add an output for the fee
        // and adjust the transaction details to match here.
        // Airbitz doesn't charge fees yet, so this is disabled.
    }

    result = std::move(out);
    return Status();
}

Status
outputsFinalize(bc::transaction_output_list &outputs,
    uint64_t change, const std::string &changeAddress)
{
    // Add change:
    if (outputDust <= change)
    {
        bc::transaction_output_type output;
        output.value = change;
        ABC_CHECK(outputScriptForAddress(output.script, changeAddress));
        outputs.push_back(output);
    }

    // Sort:
    auto compare = [](const bc::transaction_output_type &a,
        const bc::transaction_output_type &b)
    {
        return a.value < b.value;
    };
    std::sort(outputs.begin(), outputs.end(), compare);

    // Check for dust:
    for (const auto &output: outputs)
        if (output.value < outputDust)
            return ABC_ERROR(ABC_CC_InsufficientFunds, "Trying to send dust");

    return Status();
}

uint64_t
outputsTotal(const bc::transaction_output_list &outputs)
{
    int64_t out = 0;
    for (const auto &output: outputs)
        out += output.value;
    return out;
}

} // namespace abcd
