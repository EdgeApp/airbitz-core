/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_SPEND_SPEND_HPP
#define ABCD_SPEND_SPEND_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include "../wallet/TxMetadata.hpp"
#include <bitcoin/bitcoin.hpp>
#include <map>
#include <set>

namespace abcd {

class PaymentRequest;
class Wallet;

/**
 * Options for sending money.
 */
class Spend
{
public:
    Spend(Wallet &wallet);

    /**
     * Send money to this address.
     */
    Status
    addAddress(const std::string &address, uint64_t amount);

    /**
     * Send money to this BIP70 payment request.
     */
    Status
    addPaymentRequest(PaymentRequest *request);

    /**
     * Transfer money to this other Airbitz wallet.
     * @param metadata Information to be saved in the target wallet.
     */
    Status
    addTransfer(Wallet &target, uint64_t amount, TxMetadata metadata);

    /**
     * Provides metadata to be saved alongside the transaction.
     */
    Status
    metadataSet(const TxMetadata &metadata);

    /**
     * Calculate the fees that will be required to perform this send.
     */
    Status
    calculateFees(uint64_t &totalFees);

    /**
     * Calculate the total amount that could be sent to these outputs.
     */
    Status
    calculateMax(uint64_t &maxSatoshi);

    /**
     * Builds a signed transaction.
     */
    Status
    signTx(DataChunk &result);

    /**
     * Broadcasts a transaction to the bitcoin network,
     * optionally passing through the payment protocol server first.
     */
    Status
    broadcastTx(DataSlice rawTx);

    /**
     * Saves a transaction to the wallet.
     */
    Status
    saveTx(DataSlice rawTx, std::string &txidOut);

private:
    Wallet &wallet_;
    std::map<std::string, uint64_t> addresses_;
    std::set<PaymentRequest *> paymentRequests_;
    std::map<Wallet *, TxMetadata> transfers_;

    uint64_t airbitzFeeSent_;
    TxMetadata metadata_;

    Status
    makeOutputs(bc::transaction_output_list &result);

    Status
    makeTx(libbitcoin::transaction_type &result,
           const std::string &changeAddress);
};

} // namespace abcd

#endif
