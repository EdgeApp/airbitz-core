/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Spend.hpp"
#include "AirbitzFee.hpp"
#include "Broadcast.hpp"
#include "Inputs.hpp"
#include "Outputs.hpp"
#include "PaymentProto.hpp"
#include "../Context.hpp"
#include "../General.hpp"
#include "../bitcoin/cache/Cache.hpp"
#include "../bitcoin/Utility.hpp"
#include "../exchange/ExchangeCache.hpp"
#include "../util/Debug.hpp"
#include "../wallet/Wallet.hpp"

namespace abcd {

Spend::Spend(Wallet &wallet):
    wallet_(wallet),
    airbitzFeePending_(wallet_.txs.airbitzFeePending()),
    feeLevel_(ABC_SpendFeeLevelStandard)
{
}

Status
Spend::addAddress(const std::string &address, uint64_t amount)
{
    addresses_[address] += amount;
    return Status();
}

Status
Spend::addPaymentRequest(PaymentRequest *request)
{
    paymentRequests_.insert(request);
    return Status();
}

Status
Spend::addTransfer(Wallet &target, uint64_t amount, Metadata metadata)
{
    // Create a new address to spend to:
    AddressMeta address;
    ABC_CHECK(target.addresses.getNew(address));
    addresses_[address.address] += amount;

    // Adjust and save the metadata:
    if (!metadata.amountCurrency)
    {
        ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                      metadata.amountCurrency, amount,
                      static_cast<Currency>(target.currency())));
    }
    transfers_[&target] = metadata;

    return Status();
}

Status
Spend::metadataSet(const Metadata &metadata)
{
    metadata_ = metadata;
    return Status();
}

Status
Spend::feeSet(tABC_SpendFeeLevel feeLevel, uint64_t customFeeSatoshi)
{
    feeLevel_ = feeLevel;
    customFeeSatoshi_ = customFeeSatoshi;
    return Status();
}

Status
Spend::calculateFees(uint64_t &totalFees)
{
    // Make an unsigned transaction:
    AddressMeta changeAddress;
    ABC_CHECK(wallet_.addresses.getNew(changeAddress));
    bc::transaction_type tx;
    ABC_CHECK(makeTx(tx, changeAddress.address));

    // Calculate the miner fee:
    TxInfo info;
    ABC_CHECK(wallet_.cache.txs.info(info, tx));

    totalFees = airbitzFeeSent_ + info.fee;
    return Status();
}

Status
Spend::calculateMax(uint64_t &maxSatoshi)
{
    const auto addresses = wallet_.addresses.list();
    const auto utxos = wallet_.cache.txs.utxos(addresses);
    const auto info = generalAirbitzFeeInfo();

    // Set up a fake transaction:
    bc::transaction_type tx;
    tx.version = 1;
    tx.locktime = 0;

    // Set up our basic output list:
    bc::transaction_output_list outputs;
    ABC_CHECK(makeOutputs(outputs));
    for (auto &output: outputs)
        output.value = 0;

    // We can't send anything to an empty list:
    if (outputs.empty())
    {
        logInfo("Aborting max calculation due to empty output list.");
        maxSatoshi = 0;
        return Status();
    }

    // The range for our binary search (min <= result < max)
    int64_t min = 0;
    int64_t max = 0;
    for (const auto utxo: utxos)
        max += utxo.value;
    max += 1; // Make this a non-inclusive maximum

    logInfo("Max spend search: min: " + std::to_string(min) +
            ", max: " + std::to_string(max) +
            ", utxo count: " + std::to_string(utxos.size()) +
            ", address count: " + std::to_string(addresses.size()));

    // Do the binary search:
    while (min + 1 < max)
    {
        int64_t guess = (min + max) / 2;
        tx.outputs = outputs;
        tx.outputs[0].value = guess;
        ABC_CHECK(addAirbitzFeeOutput(tx.outputs, info));

        uint64_t fee, change;
        if (inputsPickOptimal(fee, change, tx, filterOutputs(utxos),
                              feeLevel_, customFeeSatoshi_))
            min = guess;
        else
            max = guess;
    }

    maxSatoshi = min;
    return Status();
}

Status
Spend::signTx(DataChunk &result)
{
    // Make an unsigned transaction:
    AddressMeta changeAddress;
    ABC_CHECK(wallet_.addresses.getNew(changeAddress));
    bc::transaction_type tx;
    ABC_CHECK(makeTx(tx, changeAddress.address));

    // Sign the transaction:
    KeyTable keys = wallet_.addresses.keyTable();
    ABC_CHECK(abcd::signTx(tx, wallet_.cache.txs, keys));
    result.resize(satoshi_raw_size(tx));
    bc::satoshi_save(tx, result.begin());

    ABC_DebugLog(bc::pretty(tx).c_str());

    return Status();
}

Status
Spend::broadcastTx(DataSlice rawTx)
{
    // Let the merchant broadcast the transaction:
    for (auto request: paymentRequests_)
    {
        // TODO: Update the metadata with something about a refund
        AddressMeta refundAddress;
        ABC_CHECK(wallet_.addresses.getNew(refundAddress));
        refundAddress.time = time(nullptr);
        refundAddress.metadata = metadata_;
        ABC_CHECK(wallet_.addresses.save(refundAddress));

        bc::script_type refundScript;
        ABC_CHECK(outputScriptForAddress(refundScript, refundAddress.address));
        DataChunk refund = save_script(refundScript);

        PaymentReceipt receipt;
        ABC_CHECK(request->pay(receipt, rawTx, refund));

        // Append the receipt memo to the notes field:
        if (receipt.ack.has_memo())
        {
            std::string notes = metadata_.notes;
            if (!notes.empty())
                notes += '\n';
            notes += receipt.ack.memo();
            metadata_.notes = notes;
        }
    }

    // Send to the network:
    ABC_CHECK(abcd::broadcastTx(wallet_, rawTx));

    // TODO: Return success if at least one broadcast has succeeds.
    // Right now, a partial success would lead to an unsaved transaction.
    return Status();
}

Status
Spend::saveTx(DataSlice rawTx, std::string &txidOut)
{
    bc::transaction_type tx;
    ABC_CHECK(decodeTx(tx, rawTx));

    // Calculate transaction amounts:
    TxInfo info;
    ABC_CHECK(wallet_.cache.txs.info(info, tx));
    auto balance = wallet_.addresses.balance(info);

    // Create Airbitz metadata:
    TxMeta meta;
    meta.ntxid = info.ntxid;
    meta.txid = info.txid;
    meta.timeCreation = time(nullptr);
    meta.internal = true;
    meta.airbitzFeeWanted = airbitzFeeWanted_;
    meta.airbitzFeeSent = airbitzFeeSent_;
    meta.metadata = metadata_;

    logInfo("Airbitz fee: " +
            std::to_string(airbitzFeeWanted_) + " wanted, " +
            std::to_string(wallet_.txs.airbitzFeePending()) + " pending, " +
            std::to_string(airbitzFeeSent_) + " sent");

    // Calculate amountCurrency if necessary:
    if (!meta.metadata.amountCurrency)
    {
        ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                      meta.metadata.amountCurrency, balance,
                      static_cast<Currency>(wallet_.currency())));
    }

    // Save the transaction:
    ABC_CHECK(wallet_.txs.save(meta, balance, info.fee));

    // Update any transfers:
    for (auto transfer: transfers_)
    {
        meta.airbitzFeeSent = 0;
        meta.metadata = transfer.second;
        transfer.first->txs.save(meta, -balance, info.fee);
    }

    // Update the transaction cache:
    wallet_.cache.txs.insert(tx);
    wallet_.cache.addresses.updateSpend(info);
    wallet_.cache.save().log(); // Failure is fine

    txidOut = info.txid;
    return Status();
}

Status
Spend::makeOutputs(bc::transaction_output_list &result)
{
    bc::transaction_output_list out;

    // Create outputs for normal addresses:
    for (const auto &i: addresses_)
    {
        bc::transaction_output_type output;
        output.value = i.second;
        ABC_CHECK(outputScriptForAddress(output.script, i.first));
        out.push_back(output);
    }

    // Read in BIP 70 outputs:
    for (auto request: paymentRequests_)
    {
        for (auto &a: request->outputs())
        {
            bc::transaction_output_type output;
            output.value = a.amount;
            output.script = bc::parse_script(bc::to_data_chunk(a.script));
            out.push_back(output);
        }
    }

    result = std::move(out);
    return Status();
}

Status
Spend::addAirbitzFeeOutput(bc::transaction_output_list &outputs,
                           const AirbitzFeeInfo &info)
{
    // Calculate the Airbitz fee we owe:
    if (transfers_.empty())
        airbitzFeeWanted_ = airbitzFeeOutgoing(info, outputsTotal(outputs));
    else
        airbitzFeeWanted_ = 0;

    airbitzFeeSent_ = airbitzFeePending_ + airbitzFeeWanted_;
    if (airbitzFeeSent_ < info.sendMin || info.addresses.empty())
        airbitzFeeSent_ = 0;

    // Add a fee output if it makes sense:
    if (airbitzFeeSent_)
    {
        auto i = info.addresses.begin();
        std::advance(i, time(nullptr) % info.addresses.size());

        bc::transaction_output_type output;
        output.value = airbitzFeeSent_;
        ABC_CHECK(outputScriptForAddress(output.script, *i));
        outputs.push_back(output);
    }

    return Status();
}

Status
Spend::makeTx(libbitcoin::transaction_type &result,
              const std::string &changeAddress)
{
    bc::transaction_type tx;
    tx.version = 1;
    tx.locktime = 0;
    ABC_CHECK(makeOutputs(tx.outputs));
    const auto info = generalAirbitzFeeInfo();
    ABC_CHECK(addAirbitzFeeOutput(tx.outputs, info));

    // Check if enough confirmed inputs are available,
    // otherwise use unconfirmed inputs too:
    uint64_t fee, change;
    auto utxos = wallet_.cache.txs.utxos(wallet_.addresses.list());
    if (!inputsPickOptimal(fee, change, tx, filterOutputs(utxos, true),
                           feeLevel_, customFeeSatoshi_))
    {
        ABC_CHECK(inputsPickOptimal(fee, change, tx, filterOutputs(utxos),
                                    feeLevel_, customFeeSatoshi_));
    }

    ABC_CHECK(outputsFinalize(tx.outputs, change, changeAddress));

    result = std::move(tx);
    return Status();
}

} // namespace abcd
