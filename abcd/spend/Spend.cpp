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
#include "../util/Util.hpp"
#include "../wallet/Details.hpp"
#include "../wallet/Wallet.hpp"
#include <bitcoin/bitcoin.hpp>

namespace abcd {

#define NO_AB_FEES

static Status
getUtxos(bc::output_info_list &result, const Wallet &self)
{
    auto addresses = self.addresses.list();
    result = self.txdb.get_utxos(
        AddressSet(addresses.begin(), addresses.end()), true);
    return Status();
}

static Status
spendMakeTx(libbitcoin::transaction_type &result, Wallet &self,
    SendInfo *pInfo, const std::string &changeAddress)
{
    bc::output_info_list utxos;
    ABC_CHECK(getUtxos(utxos, self));

    AutoFree<tABC_GeneralInfo, ABC_GeneralFreeInfo> pFeeInfo;
    ABC_CHECK_OLD(ABC_GeneralGetInfo(&pFeeInfo.get(), &error));

    bc::transaction_type tx;
    tx.version = 1;
    tx.locktime = 0;
    ABC_CHECK(outputsForSendInfo(tx.outputs, pInfo));

    uint64_t fee, change;
    ABC_CHECK(inputsPickOptimal(fee, change, tx, utxos, pFeeInfo));
    ABC_CHECK(outputsFinalize(tx.outputs, change, changeAddress));
    pInfo->metadata.amountFeesMinersSatoshi = fee;

    result = std::move(tx);
    return Status();
}

SendInfo::~SendInfo()
{
    ABC_FREE_STR(szDestAddress);
    delete paymentRequest;
}

SendInfo::SendInfo()
{
    szDestAddress = nullptr;
    paymentRequest = nullptr;
    bTransfer = false;
}

tABC_CC  ABC_TxCalcSendFees(Wallet &self, SendInfo *pInfo, uint64_t *pTotalFees, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);
    bc::transaction_type tx;
    Address changeAddress;

    ABC_CHECK_NULL(pInfo);

    pInfo->metadata.amountFeesAirbitzSatoshi = 0;
    pInfo->metadata.amountFeesMinersSatoshi = 0;

    // Make an unsigned transaction
    ABC_CHECK_NEW(self.addresses.getNew(changeAddress));
    ABC_CHECK_NEW(spendMakeTx(tx, self, pInfo, changeAddress.address));

    *pTotalFees = pInfo->metadata.amountFeesAirbitzSatoshi
                + pInfo->metadata.amountFeesMinersSatoshi;

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
        bc::output_info_list utxos;
        ABC_CHECK_NEW(getUtxos(utxos, self));

        AutoFree<tABC_GeneralInfo, ABC_GeneralFreeInfo> pFeeInfo;
        ABC_CHECK_RET(ABC_GeneralGetInfo(&pFeeInfo.get(), pError));

        bc::transaction_type tx;
        tx.version = 1;
        tx.locktime = 0;

        auto oldAmount = pInfo->metadata.amountSatoshi;
        pInfo->metadata.amountSatoshi = 0;
        ABC_CHECK_NEW(outputsForSendInfo(tx.outputs, pInfo));
        pInfo->metadata.amountSatoshi = oldAmount;

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
                   char             **pszNtxid,
                   tABC_Error       *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    AutoCoreLock lock(gCoreMutex);

    Address changeAddress;
    bc::transaction_type tx;

    ABC_CHECK_NULL(pInfo);

    // Make an unsigned transaction:
    ABC_CHECK_NEW(self.addresses.getNew(changeAddress));
    ABC_CHECK_NEW(spendMakeTx(tx, self, pInfo, changeAddress.address));

    // Sign and send transaction:
    {
        // Sign the transaction:
        KeyTable keys = self.addresses.keyTable();
        ABC_CHECK_NEW(signTx(tx, self, keys));

        ABC_DebugLog("Change: %s, Amount: %s, Contents: %s",
            changeAddress.address.c_str(),
            std::to_string(pInfo->metadata.amountSatoshi).c_str(),
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
            refundAddress.metadata = pInfo->metadata;
            ABC_CHECK_NEW(self.addresses.save(refundAddress));

            bc::script_type refundScript;
            ABC_CHECK_NEW(outputScriptForAddress(refundScript, refundAddress.address));
            DataChunk refund = save_script(refundScript);

            PaymentReceipt receipt;
            ABC_CHECK_NEW(pInfo->paymentRequest->pay(receipt, rawTx, refund));

            // Append the receipt memo to the notes field:
            if (receipt.ack.has_memo())
            {
                std::string notes = pInfo->metadata.notes;
                if (!notes.empty())
                    notes += '\n';
                notes += receipt.ack.memo();
                pInfo->metadata.notes = notes;
            }
        }

        // Mark the outputs as spent:
        watcherSend(self, tx).log();
    }

    // Update the ABC db
    {
        auto txid = bc::encode_hash(bc::hash_transaction(tx));
        auto ntxid = ABC_BridgeNonMalleableTxId(tx);

        std::vector<std::string> addresses;
        for (const auto &output: tx.outputs)
        {
            bc::payment_address addr;
            bc::extract(addr, output.script);
            addresses.push_back(addr.encoded());
        }
        ABC_CHECK_RET(ABC_TxSendComplete(self, pInfo, ntxid, txid, addresses, pError));

        // return the new tx id
        *pszNtxid = stringCopy(ntxid);
    }

exit:
    return cc;
}

} // namespace abcd
