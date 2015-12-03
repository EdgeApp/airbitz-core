/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "Outputs.hpp"
#include "PaymentProto.hpp"
#include "Spend.hpp"
#include "../General.hpp"
#include "../bitcoin/Testnet.hpp"
#include <iterator>

#define MINIMUM_DUST_THRESHOLD 4000

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
outputsForSendInfo(bc::transaction_output_list &result, SendInfo *pInfo)
{
    bc::transaction_output_list out;

    if (pInfo->paymentRequest)
    {
        // Gather the outputs from the payment request, if any:
        for (auto &a: pInfo->paymentRequest->outputs())
        {
            bc::transaction_output_type output;
            output.value = a.amount;
            output.script = bc::parse_script(bc::to_data_chunk(a.script));
            out.push_back(output);
        }
    }
    else
    {
        // Otherwise, make an output for the ordinary address:
        bc::transaction_output_type output;
        output.value = pInfo->metadata.amountSatoshi;
        ABC_CHECK(outputScriptForAddress(output.script, pInfo->destAddress));
        out.push_back(output);
    }

    // Handle the Airbitz fees:
    pInfo->metadata.amountFeesAirbitzSatoshi = 0;
    const auto info = generalAirbitzFeeInfo();
    int64_t airbitzFee = info.rate * outputsTotal(out);
    if (airbitzFee < info.minSatoshi)
        airbitzFee = info.minSatoshi;
    if (airbitzFee > info.maxSatoshi)
        airbitzFee = info.maxSatoshi;
    if (airbitzFee && !pInfo->bTransfer)
    {
        auto i = info.addresses.begin();
        std::advance(i, rand() % info.addresses.size());

        bc::transaction_output_type output;
        output.value = airbitzFee;
        ABC_CHECK(outputScriptForAddress(output.script, *i));
        out.push_back(output);

        pInfo->metadata.amountFeesAirbitzSatoshi = airbitzFee;
    }

    result = std::move(out);
    return Status();
}

bool
outputIsDust(uint64_t amount)
{
//    return amount < 546;
    return amount < MINIMUM_DUST_THRESHOLD;
}

Status
outputsFinalize(bc::transaction_output_list &outputs,
    uint64_t change, const std::string &changeAddress)
{
    // Add change:
    if (!outputIsDust(change))
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
        if (outputIsDust(output.value))
            return ABC_ERROR(ABC_CC_SpendDust, "Trying to send dust");

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
