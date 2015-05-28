/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "Spend.hpp"
#include "Broadcast.hpp"
#include "Inputs.hpp"
#include "Outputs.hpp"
#include "PaymentProto.hpp"
#include "../Tx.hpp"
#include "../bitcoin/Watcher.hpp"
#include "../bitcoin/WatcherBridge.hpp"
#include "../util/Mutex.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

#define NO_AB_FEES

static Status
spendMakeTx(libbitcoin::transaction_type &result, tABC_WalletID self,
    tABC_TxSendInfo *pInfo, const std::string &changeAddress)
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
ABC_BridgeExtractOutputs(tABC_WalletID self, tABC_UnsavedTx **ppUtx,
                         const libbitcoin::transaction_type &tx,
                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    auto txid = bc::encode_hex(bc::hash_transaction(tx));
    auto ntxid = ABC_BridgeNonMalleableTxId(tx);
    int i = 0;
    AutoFree<tABC_UnsavedTx, ABC_UnsavedTxFree> pUtx;

    Watcher *watcher = nullptr;
    ABC_CHECK_NEW(watcherFind(watcher, self), pError);

    // Fill in tABC_UnsavedTx structure:
    ABC_NEW(pUtx.get(), tABC_UnsavedTx);
    ABC_STRDUP(pUtx->szTxId, ntxid.c_str());
    ABC_STRDUP(pUtx->szTxMalleableId, txid.c_str());
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
        ABC_STRDUP(out->szTxId, bc::encode_hex(prev.hash).c_str());
        ABC_STRDUP(out->szAddress, addr.encoded().c_str());

        auto tx = watcher->find_tx(prev.hash);
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
        ABC_STRDUP(out->szTxId, txid.c_str());
        ABC_STRDUP(out->szAddress, addr.encoded().c_str());

        pUtx->aOutputs[i] = out;
        i++;
    }

    *ppUtx = pUtx.get();
    pUtx.get() = 0;

exit:
    return cc;
}

/**
 * Free a send info struct
 */
void ABC_TxSendInfoFree(tABC_TxSendInfo *pTxSendInfo)
{
    if (pTxSendInfo)
    {
        ABC_FREE_STR(pTxSendInfo->szDestAddress);
        delete pTxSendInfo->paymentRequest;
        ABC_TxFreeDetails(pTxSendInfo->pDetails);
        ABC_WalletIDFree(pTxSendInfo->walletDest);

        ABC_CLEAR_FREE(pTxSendInfo, sizeof(tABC_TxSendInfo));
    }
}

/**
 * Allocate a send info struct and populate it with the data given
 */
tABC_CC ABC_TxSendInfoAlloc(tABC_TxSendInfo **ppTxSendInfo,
                            const char *szDestAddress,
                            const tABC_TxDetails *pDetails,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxSendInfo *pTxSendInfo = NULL;

    ABC_CHECK_NULL(ppTxSendInfo);
    ABC_CHECK_NULL(pDetails);

    ABC_NEW(pTxSendInfo, tABC_TxSendInfo);
    if (szDestAddress)
        ABC_STRDUP(pTxSendInfo->szDestAddress, szDestAddress);
    ABC_CHECK_RET(ABC_TxDupDetails(&(pTxSendInfo->pDetails), pDetails, pError));

    *ppTxSendInfo = pTxSendInfo;
    pTxSendInfo = NULL;

exit:
    ABC_TxSendInfoFree(pTxSendInfo);

    return cc;
}

tABC_CC  ABC_TxCalcSendFees(tABC_WalletID self, tABC_TxSendInfo *pInfo, uint64_t *pTotalFees, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    std::string changeAddress;
    bc::transaction_type tx;

    ABC_CHECK_NULL(pInfo);

    pInfo->pDetails->amountFeesAirbitzSatoshi = 0;
    pInfo->pDetails->amountFeesMinersSatoshi = 0;

    // Make an unsigned transaction
    ABC_CHECK_NEW(txNewChangeAddress(changeAddress, self, pInfo->pDetails), pError);
    ABC_CHECK_NEW(spendMakeTx(tx, self, pInfo, changeAddress), pError);

    *pTotalFees = pInfo->pDetails->amountFeesAirbitzSatoshi
                + pInfo->pDetails->amountFeesMinersSatoshi;

exit:
    return cc;
}

tABC_CC ABC_BridgeMaxSpendable(tABC_WalletID self,
                               tABC_TxSendInfo *pInfo,
                               uint64_t *pMaxSatoshi,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    {
        Watcher *watcher = nullptr;
        ABC_CHECK_NEW(watcherFind(watcher, self), pError);
        auto utxos = watcher->get_utxos(true);

        AutoFree<tABC_GeneralInfo, ABC_GeneralFreeInfo> pFeeInfo;
        ABC_CHECK_RET(ABC_GeneralGetInfo(&pFeeInfo.get(), pError));

        bc::transaction_type tx;
        tx.version = 1;
        tx.locktime = 0;

        auto oldAmount = pInfo->pDetails->amountSatoshi;
        pInfo->pDetails->amountSatoshi = 0;
        ABC_CHECK_NEW(outputsForSendInfo(tx.outputs, pInfo), pError);
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
tABC_CC ABC_TxSend(tABC_WalletID self,
                   tABC_TxSendInfo  *pInfo,
                   char             **pszTxID,
                   tABC_Error       *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    std::string changeAddress;
    bc::transaction_type tx;
    AutoFree<tABC_UnsavedTx, ABC_UnsavedTxFree> unsaved;

    ABC_CHECK_NULL(pInfo);

    // Make an unsigned transaction:
    ABC_CHECK_NEW(txNewChangeAddress(changeAddress, self, pInfo->pDetails), pError);
    ABC_CHECK_NEW(spendMakeTx(tx, self, pInfo, changeAddress), pError);

    // Sign and send transaction:
    {
        Watcher *watcher = nullptr;
        ABC_CHECK_NEW(watcherFind(watcher, self), pError);

        // Sign the transaction:
        KeyTable keys;
        ABC_CHECK_NEW(txKeyTableGet(keys, self), pError);
        ABC_CHECK_NEW(signTx(tx, *watcher, keys), pError);

        ABC_DebugLog("Change: %s, Amount: %ld, Contents: %s",
            changeAddress.c_str(), pInfo->pDetails->amountSatoshi,
            bc::pretty(tx).c_str());

        // Send to the network:
        bc::data_chunk rawTx(satoshi_raw_size(tx));
        bc::satoshi_save(tx, rawTx.begin());
        ABC_CHECK_NEW(broadcastTx(rawTx), pError);

        // Let the merchant broadcast the transaction:
        if (pInfo->paymentRequest)
        {
            // TODO: add something to the details about a refund
            AutoFree<tABC_TxDetails, ABC_TxFreeDetails> pRefundDetails;
            ABC_CHECK_RET(ABC_TxDupDetails(&pRefundDetails.get(), pInfo->pDetails, pError));

            std::string refundAddress;
            ABC_CHECK_NEW(txNewChangeAddress(refundAddress, self, pRefundDetails), pError);

            bc::script_type refundScript;
            ABC_CHECK_NEW(outputScriptForAddress(refundScript, refundAddress), pError);
            DataChunk refund = save_script(refundScript);

            PaymentReceipt receipt;
            ABC_CHECK_NEW(pInfo->paymentRequest->pay(receipt, rawTx, refund), pError);

            // TODO: make something happen with the memo???
            if (receipt.ack.has_memo())
                ABC_DebugLog("Memo: %s", receipt.ack.memo().c_str());
        }

        // Mark the outputs as spent:
        watcher->send_tx(tx);
        watcherSave(self); // Failure is not fatal
    }

    // Update the ABC db
    ABC_CHECK_RET(ABC_BridgeExtractOutputs(self, &unsaved.get(), tx, pError));
    ABC_CHECK_RET(ABC_TxSendComplete(self, pInfo, unsaved, pError));

    // return the new tx id
    ABC_STRDUP(*pszTxID, unsaved->szTxId);

exit:
    return cc;
}

} // namespace abcd
