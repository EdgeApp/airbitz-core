/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_SPEND_SPEND_HPP
#define ABCD_SPEND_SPEND_HPP

#include "../../util/Data.hpp"
#include "../../util/Status.hpp"
#include "../../wallet/Metadata.hpp"
#include <bitcoin/bitcoin.hpp>
#include <map>
#include <set>

namespace abcd {

struct AirbitzFeeInfo;
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
    addTransfer(Wallet &target, uint64_t amount, Metadata metadata);

    /**
     * Provides metadata to be saved alongside the transaction.
     */
    Status
    metadataSet(const Metadata &metadata);

    /**
     * Change the mining fee level of this Spend object
     */
    Status
    feeSet(tABC_SpendFeeLevel feeLevel, uint64_t customFeeSatoshi);

    /**
     * Calculate the fees that will be required to perform this send.
     */
    Status
    calculateFees(uint64_t &totalFees, bool skipUnconfirmed=false);

    /**
     * Calculate the total amount that could be sent to these outputs.
     */
    Status
    calculateMax(uint64_t &maxSatoshi, bool skipUnconfirmed=false);

    /**
     * Builds a signed transaction.
     */
    Status
    signTx(DataChunk &result, bool skipUnconfirmed=false);

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
    std::map<Wallet *, Metadata> transfers_;

    uint64_t airbitzFeePending_;
    uint64_t airbitzFeeWanted_;
    uint64_t airbitzFeeSent_;

    Metadata metadata_;
    tABC_SpendFeeLevel feeLevel_;
    uint64_t customFeeSatoshi_;

    Status
    makeOutputs(bc::transaction_output_list &result);

    Status
    addAirbitzFeeOutput(bc::transaction_output_list &outputs,
                        const AirbitzFeeInfo &info);

    Status
    makeTx(libbitcoin::transaction_type &result,
           const std::string &changeAddress, bool skipUnconfirmed=false);
};

} // namespace abcd

#endif
