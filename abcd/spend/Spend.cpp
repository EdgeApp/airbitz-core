/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#include "Spend.hpp"
#include "Broadcast.hpp"
#include "Inputs.hpp"
#include "../Tx.hpp"
#include "../bitcoin/Watcher.hpp"
#include "../bitcoin/WatcherBridge.hpp"
#include "../util/Mutex.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

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
    ABC_CHECK_NULL(szDestAddress);
    ABC_CHECK_NULL(pDetails);

    ABC_NEW(pTxSendInfo, tABC_TxSendInfo);

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
    cc = ABC_BridgeTxMake(self, pInfo, changeAddress, tx, pError);

    *pTotalFees = pInfo->pDetails->amountFeesAirbitzSatoshi
                + pInfo->pDetails->amountFeesMinersSatoshi;
    ABC_CHECK_RET(cc);

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
    ABC_CHECK_RET(ABC_BridgeTxMake(self, pInfo, changeAddress, tx, pError));

    // Sign and send transaction:
    {
        Watcher *watcher = nullptr;
        ABC_CHECK_NEW(watcherFind(watcher, self), pError);

        // Sign the transaction:
        KeyTable keys;
        ABC_CHECK_NEW(txKeyTableGet(keys, self), pError);
        ABC_CHECK_NEW(signTx(tx, *watcher, keys), pError);

        // Send to the network:
        bc::data_chunk rawTx(satoshi_raw_size(tx));
        bc::satoshi_save(tx, rawTx.begin());
        ABC_CHECK_NEW(broadcastTx(rawTx), pError);

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
