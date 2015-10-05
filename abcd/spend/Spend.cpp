/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "Spend.hpp"
#include "Broadcast.hpp"
#include "Inputs.hpp"
#include "Outputs.hpp"
#include "PaymentProto.hpp"
#include "../General.hpp"
#include "../Tx.hpp"
#include "../bitcoin/Watcher.hpp"
#include "../bitcoin/WatcherBridge.hpp"
#include "../util/Mutex.hpp"
#include "../wallet/Details.hpp"
#include "../wallet/Wallet.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

#define NO_AB_FEES

static Status
spendMakeTx(libbitcoin::transaction_type &result, Wallet &self,
    SendInfo *pInfo, const std::string &changeAddress)
{
    Watcher *watcher = nullptr;
    ABC_CHECK(watcherFind(watcher, self));
    auto utxos = watcher->get_utxos(true);

    AutoFree<tABC_GeneralInfo, ABC_GeneralFreeInfo> pFeeInfo;
    ABC_CHECK_OLD(ABC_GeneralGetInfo(&pFeeInfo.get(), &error));

    bc::transaction_type tx;
    tx.version = 1;
    tx.locktime = 0;
    ABC_CHECK(outputsForSendInfo(tx.outputs, pInfo));

    uint64_t fee, change;
    ABC_CHECK(inputsPickOptimal(fee, change, tx, utxos, pFeeInfo));
    ABC_CHECK(outputsFinalize(tx.outputs, change, changeAddress));
    pInfo->pDetails->amountFeesMinersSatoshi = fee;

    result = std::move(tx);
    return Status();
}

/**
 * Fills in the tABC_UnsavedTx structure.
 */
static tABC_CC
ABC_BridgeExtractOutputs(Wallet &self, tABC_UnsavedTx **ppUtx,
                         const libbitcoin::transaction_type &tx,
                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    auto txid = bc::encode_hash(bc::hash_transaction(tx));
    auto ntxid = ABC_BridgeNonMalleableTxId(tx);
    int i = 0;
    AutoFree<tABC_UnsavedTx, ABC_UnsavedTxFree>
        pUtx(structAlloc<tABC_UnsavedTx>());

    Watcher *watcher = nullptr;
    ABC_CHECK_NEW(watcherFind(watcher, self));

    // Fill in tABC_UnsavedTx structure:
    pUtx->szTxId = stringCopy(ntxid);
    pUtx->szTxMalleableId = stringCopy(txid);
    pUtx->countOutputs = tx.inputs.size() + tx.outputs.size();
    ABC_ARRAY_NEW(pUtx->aOutputs, pUtx->countOutputs, tABC_TxOutput*)

    // Build output entries:
    for (const auto &input: tx.inputs)
    {
        auto prev = input.previous_output;
        bc::payment_address addr;
        bc::extract(addr, input.script);

        tABC_TxOutput *out = (tABC_TxOutput *) malloc(sizeof(tABC_TxOutput));
        out->input = true;
        out->szTxId = stringCopy(bc::encode_hash(prev.hash));
        out->szAddress = stringCopy(addr.encoded());

        auto tx = watcher->find_tx_hash(prev.hash);
        if (prev.index < tx.outputs.size())
        {
            out->value = tx.outputs[prev.index].value;
        }
        pUtx->aOutputs[i] = out;
        i++;
    }
    for (const auto &output: tx.outputs)
    {
        bc::payment_address addr;
        bc::extract(addr, output.script);

        tABC_TxOutput *out = (tABC_TxOutput *) malloc(sizeof(tABC_TxOutput));
        out->input = false;
        out->value = output.value;
        out->szTxId = stringCopy(txid);
        out->szAddress = stringCopy(addr.encoded());

        pUtx->aOutputs[i] = out;
        i++;
    }

    *ppUtx = pUtx.release();

exit:
    return cc;
}

SendInfo::~SendInfo()
{
    ABC_FREE_STR(szDestAddress);
    delete paymentRequest;
    ABC_TxDetailsFree(pDetails);
}

SendInfo::SendInfo()
{
    szDestAddress = nullptr;
    paymentRequest = nullptr;
    pDetails = nullptr;
    bTransfer = false;
}

tABC_CC  ABC_TxCalcSendFees(Wallet &self, SendInfo *pInfo, uint64_t *pTotalFees, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    bc::transaction_type tx;
    Address changeAddress;

    ABC_CHECK_NULL(pInfo);

    pInfo->pDetails->amountFeesAirbitzSatoshi = 0;
    pInfo->pDetails->amountFeesMinersSatoshi = 0;

    // Make an unsigned transaction
    ABC_CHECK_NEW(self.addresses.getNew(changeAddress));
    ABC_CHECK_NEW(spendMakeTx(tx, self, pInfo, changeAddress.address));

    *pTotalFees = pInfo->pDetails->amountFeesAirbitzSatoshi
                + pInfo->pDetails->amountFeesMinersSatoshi;

exit:
    return cc;
}

tABC_CC ABC_BridgeMaxSpendable(Wallet &self,
                               SendInfo *pInfo,
                               uint64_t *pMaxSatoshi,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    {
        Watcher *watcher = nullptr;
        ABC_CHECK_NEW(watcherFind(watcher, self));
        auto utxos = watcher->get_utxos(true);

        AutoFree<tABC_GeneralInfo, ABC_GeneralFreeInfo> pFeeInfo;
        ABC_CHECK_RET(ABC_GeneralGetInfo(&pFeeInfo.get(), pError));

        bc::transaction_type tx;
        tx.version = 1;
        tx.locktime = 0;

        auto oldAmount = pInfo->pDetails->amountSatoshi;
        pInfo->pDetails->amountSatoshi = 0;
        ABC_CHECK_NEW(outputsForSendInfo(tx.outputs, pInfo));
        pInfo->pDetails->amountSatoshi = oldAmount;

        uint64_t fee, change;
        if (inputsPickMaximum(fee, change, tx, utxos, pFeeInfo))
            *pMaxSatoshi = change;
        else
            *pMaxSatoshi = 0;
    }

exit:
    return cc;
}

/**
 * Sends the transaction with the given info.
 *
 * @param pInfo Pointer to transaction information
 * @param pszTxID Pointer to hold allocated pointer to transaction ID string
 */
tABC_CC ABC_TxSend(Wallet &self,
                   SendInfo         *pInfo,
                   char             **pszTxID,
                   tABC_Error       *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    Address changeAddress;
    bc::transaction_type tx;
    AutoFree<tABC_UnsavedTx, ABC_UnsavedTxFree> unsaved;

    ABC_CHECK_NULL(pInfo);

    // Make an unsigned transaction:
    ABC_CHECK_NEW(self.addresses.getNew(changeAddress));
    ABC_CHECK_NEW(spendMakeTx(tx, self, pInfo, changeAddress.address));

    // Sign and send transaction:
    {
        Watcher *watcher = nullptr;
        ABC_CHECK_NEW(watcherFind(watcher, self));

        // Sign the transaction:
        KeyTable keys = self.addresses.keyTable();
        ABC_CHECK_NEW(signTx(tx, *watcher, keys));

        ABC_DebugLog("Change: %s, Amount: %s, Contents: %s",
            changeAddress.address.c_str(),
            std::to_string(pInfo->pDetails->amountSatoshi).c_str(),
            bc::pretty(tx).c_str());

        // Send to the network:
        bc::data_chunk rawTx(satoshi_raw_size(tx));
        bc::satoshi_save(tx, rawTx.begin());
        ABC_CHECK_NEW(broadcastTx(rawTx));

        // Let the merchant broadcast the transaction:
        if (pInfo->paymentRequest)
        {
            // TODO: Update the metadata with something about a refund
            Address refundAddress;
            ABC_CHECK_NEW(self.addresses.getNew(refundAddress));
            refundAddress.time = time(nullptr);
            refundAddress.metadata = pInfo->pDetails;
            ABC_CHECK_NEW(self.addresses.save(refundAddress));

            bc::script_type refundScript;
            ABC_CHECK_NEW(outputScriptForAddress(refundScript, refundAddress.address));
            DataChunk refund = save_script(refundScript);

            PaymentReceipt receipt;
            ABC_CHECK_NEW(pInfo->paymentRequest->pay(receipt, rawTx, refund));

            // Append the receipt memo to the notes field:
            if (receipt.ack.has_memo())
            {
                std::string notes = pInfo->pDetails->szNotes;
                if (!notes.empty())
                    notes += '\n';
                notes += receipt.ack.memo();
                ABC_FREE_STR(pInfo->pDetails->szNotes);
                pInfo->pDetails->szNotes = stringCopy(notes);
            }
        }

        // Mark the outputs as spent:
        watcher->send_tx(tx);
        watcherSave(self); // Failure is not fatal
    }

    // Update the ABC db
    ABC_CHECK_RET(ABC_BridgeExtractOutputs(self, &unsaved.get(), tx, pError));
    ABC_CHECK_RET(ABC_TxSendComplete(self, pInfo, unsaved, pError));

    // return the new tx id
    *pszTxID = stringCopy(unsaved->szTxId);

exit:
    return cc;
}

} // namespace abcd
