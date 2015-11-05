/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */

#ifndef ABCD_SPEND_SPEND_HPP
#define ABCD_SPEND_SPEND_HPP

#include "../util/Status.hpp"
#include "../wallet/TxMetadata.hpp"

namespace abcd {

class PaymentRequest;
class Wallet;

/**
 * Options for sending money.
 */
struct SendInfo
{
    ~SendInfo();
    SendInfo();

    std::string             destAddress;
    PaymentRequest          *paymentRequest;
    TxMetadata              metadata;

    // Transfer from one wallet to another:
    bool                    bTransfer;
    Wallet                  *walletDest;

};

/**
 * Calculate the fees that will be required to perform this send.
 */
Status
spendCalculateFees(Wallet &self, SendInfo *pInfo, uint64_t &totalFees);

/**
 * Calculate the maximum amount that can be sent.
 */
Status
spendCalculateMax(Wallet &self, SendInfo *pInfo, uint64_t &maxSatoshi);

/**
 * Creates and sends a transaction for the given info.
 */
Status
spendSend(Wallet &self, SendInfo *pInfo, std::string &ntxidOut);

} // namespace abcd

#endif
