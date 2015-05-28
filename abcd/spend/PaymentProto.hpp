/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
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

Status
paymentInit(const std::string &certPath);

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
     * Returns true if the certificate chain checks out.
     */
    Status
    signatureOk();

    /**
     * Obtains the payment scripts and amounts being requested.
     */
    std::list<PaymentOutput>
    outputs() const;

    uint64_t
    amount() const;

    /**
     * Pays the payment request,
     * sending the bitcoin transaction to the server and obtaining a receipt.
     */
    Status
    pay(PaymentReceipt &result, DataSlice tx, DataSlice refund);

    std::string merchant() const { return merchant_; }
    std::string memo() const { return memo_; }

private:
    payments::PaymentRequest request_;
    payments::PaymentDetails details_;

    std::string merchant_;
    std::string memo_;
};

} // namespace abcd

#endif
