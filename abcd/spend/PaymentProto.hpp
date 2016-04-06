/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_SPEND_PAYMENT_PROTO_HPP
#define ABCD_SPEND_PAYMENT_PROTO_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"
#include "../../codegen/paymentrequest.pb.h"

#include <list>

namespace abcd {

struct PaymentOutput
{
    uint64_t amount;
    DataSlice script;
};

struct PaymentReceipt
{
    payments::PaymentACK ack;
};

/**
 * Represents a request from the bip70 payment protocol.
 */
class PaymentRequest
{
public:
    /**
     * Fetches the initial payment request from the server.
     */
    Status
    fetch(const std::string &url);

    /**
     * Returns true if the payment request is signed.
     */
    bool
    signatureExists();

    /**
     * Returns true if the certificate chain checks out.
     * Sets the result to the certificate domain name,
     * or URI authority if there is no certificate chain.
     */
    Status
    signatureOk(std::string &result, const std::string &uri);

    /**
     * Obtains the payment scripts and amounts being requested.
     */
    std::list<PaymentOutput>
    outputs() const;

    /**
     * Obtain the total of all outputs.
     */
    uint64_t
    amount() const;

    /**
     * Guesses the merchant name using a regex.
     */
    std::string
    merchant(const std::string &fallback="") const;

    /**
     * Returns true if the request has a memo field.
     */
    bool
    memoOk() const;

    /**
     * Returns the memo, if any.
     */
    std::string
    memo(const std::string &fallback="") const;

    /**
     * Pays the payment request,
     * sending the bitcoin transaction to the server and obtaining a receipt.
     */
    Status
    pay(PaymentReceipt &result, DataSlice tx, DataSlice refund);

private:
    payments::PaymentRequest request_;
    payments::PaymentDetails details_;
};

} // namespace abcd

#endif
