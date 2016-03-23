/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Spend.hpp"
#include "Broadcast.hpp"
#include "Inputs.hpp"
#include "Outputs.hpp"
#include "PaymentProto.hpp"
#include "../Context.hpp"
#include "../General.hpp"
#include "../bitcoin/TxDatabase.hpp"
#include "../bitcoin/Utility.hpp"
#include "../bitcoin/WatcherBridge.hpp"
#include "../util/Debug.hpp"
#include "../wallet/Wallet.hpp"

namespace abcd {

Spend::Spend(Wallet &wallet_):
    wallet_(wallet_)
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
Spend::addTransfer(Wallet &target, uint64_t amount, TxMetadata metadata)
{
    // Create a new address to spend to:
    Address address;
    ABC_CHECK(target.addresses.getNew(address));
    addresses_[address.address] += amount;

    // Adjust and save the metadata:
    metadata.amountSatoshi = amount;
    metadata.amountFeesMinersSatoshi = 0;
    metadata.amountFeesAirbitzSatoshi = 0;
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
Spend::metadataSet(const TxMetadata &metadata)
{
    metadata_ = metadata;
    return Status();
}

Status
Spend::calculateFees(uint64_t &totalFees)
{
    metadata_.amountFeesAirbitzSatoshi = 0;
    metadata_.amountFeesMinersSatoshi = 0;

    // Make an unsigned transaction:
    Address changeAddress;
    ABC_CHECK(wallet_.addresses.getNew(changeAddress));
    bc::transaction_type tx;
    ABC_CHECK(makeTx(tx, changeAddress.address));

    totalFees = metadata_.amountFeesAirbitzSatoshi +
                metadata_.amountFeesMinersSatoshi;

    return Status();
}

Status
Spend::calculateMax(uint64_t &maxSatoshi)
{
    auto utxos = wallet_.txdb.get_utxos(wallet_.addresses.list(), true);

    bc::transaction_type tx;
    tx.version = 1;
    tx.locktime = 0;
    ABC_CHECK(makeOutputs(tx.outputs));

    const auto info = generalAirbitzFeeInfo();
    uint64_t fee, usable;
    if (inputsPickMaximum(fee, usable, tx, utxos))
        maxSatoshi = generalAirbitzFeeSpendable(info, usable,
                                                !transfers_.empty());
    else
        maxSatoshi = 0;

    return Status();
}

Status
Spend::signTx(DataChunk &result)
{
    // Make an unsigned transaction:
    Address changeAddress;
    ABC_CHECK(wallet_.addresses.getNew(changeAddress));
    bc::transaction_type tx;
    ABC_CHECK(makeTx(tx, changeAddress.address));

    // Sign the transaction:
    KeyTable keys = wallet_.addresses.keyTable();
    ABC_CHECK(abcd::signTx(tx, wallet_.txdb, keys));
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
        Address refundAddress;
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
Spend::saveTx(DataSlice rawTx, std::string &ntxidOut)
{
    bc::transaction_type tx;
    auto deserial = bc::make_deserializer(rawTx.begin(), rawTx.end());
    bc::satoshi_load(deserial.iterator(), deserial.end(), tx);
    auto txid = bc::encode_hash(bc::hash_transaction(tx));
    auto ntxid = bc::encode_hash(makeNtxid(tx));

    // Save to the transaction cache:
    if (wallet_.txdb.insert(tx))
        watcherSave(wallet_).log(); // Failure is not fatal
    ABC_CHECK(wallet_.addresses.markOutputs(txid));

    // Calculate the amounts:
    int64_t amount, fee;
    ABC_CHECK(wallet_.txdb.ntxidAmounts(ntxid, wallet_.addresses.list(),
                                        amount, fee));

    // Create Airbitz metadata:
    Tx txInfo;
    txInfo.ntxid = ntxid;
    txInfo.txid = txid;
    txInfo.timeCreation = time(nullptr);
    txInfo.internal = true;
    txInfo.metadata = metadata_;
    txInfo.metadata.amountSatoshi = amount;
    txInfo.metadata.amountFeesMinersSatoshi = fee;

    // Calculate amountCurrency if necessary:
    if (!txInfo.metadata.amountCurrency)
    {
        ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                      txInfo.metadata.amountCurrency, amount,
                      static_cast<Currency>(wallet_.currency())));
    }

    // Save the transaction:
    ABC_CHECK(wallet_.txs.save(txInfo));
    wallet_.balanceDirty();

    // Update any transfers:
    for (auto transfer: transfers_)
    {
        txInfo.metadata = transfer.second;
        txInfo.metadata.amountFeesMinersSatoshi = fee;
        transfer.first->txs.save(txInfo);
    }

    ntxidOut = ntxid;
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

    // Create an Airbitz fee output:
    const auto info = generalAirbitzFeeInfo();
    auto airbitzFee = generalAirbitzFee(info, outputsTotal(out),
                                        !transfers_.empty());
    if (airbitzFee)
    {
        auto i = info.addresses.begin();
        std::advance(i, time(nullptr) % info.addresses.size());

        bc::transaction_output_type output;
        output.value = airbitzFee;
        ABC_CHECK(outputScriptForAddress(output.script, *i));
        out.push_back(output);
    }

    metadata_.amountFeesAirbitzSatoshi = airbitzFee;
    result = std::move(out);
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

    // Check if enough confirmed inputs are available,
    // otherwise use unconfirmed inputs too:
    uint64_t fee, change;
    auto utxos = wallet_.txdb.get_utxos(wallet_.addresses.list(), true);
    if (!inputsPickOptimal(fee, change, tx, utxos))
    {
        auto utxos = wallet_.txdb.get_utxos(wallet_.addresses.list(), false);
        ABC_CHECK(inputsPickOptimal(fee, change, tx, utxos));
    }

    ABC_CHECK(outputsFinalize(tx.outputs, change, changeAddress));

    metadata_.amountFeesMinersSatoshi = fee;
    result = std::move(tx);
    return Status();
}

} // namespace abcd
