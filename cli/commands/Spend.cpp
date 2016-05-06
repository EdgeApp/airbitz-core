/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "../Command.hpp"
#include "../../abcd/bitcoin/Text.hpp"
#include "../../abcd/spend/PaymentProto.hpp"
#include "../../abcd/spend/Spend.hpp"
#include "../../abcd/util/Util.hpp"
#include "../../abcd/wallet/Wallet.hpp"
#include <iostream>

using namespace abcd;

COMMAND(InitLevel::wallet, SpendAddress, "spend-address",
        " <address> <amount>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto address = argv[0];
    const auto amount = atol(argv[1]);

    Spend spend(*session.wallet);
    ABC_CHECK(spend.addAddress(address, amount));
    std::cout << "Sending " << amount << " satoshis to " << address
              << std::endl;

    DataChunk rawTx;
    std::string txid;
    ABC_CHECK(spend.signTx(rawTx));
    ABC_CHECK(spend.broadcastTx(rawTx));
    ABC_CHECK(spend.saveTx(rawTx, txid));
    std::cout << "Transaction id: " << txid << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, SpendBip70, "spend-bip70",
        " <uri>")
{
    if (argc != 1)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto uri = argv[0];

    PaymentRequest request;
    ABC_CHECK(request.fetch(uri));
    Metadata metadata;
    ABC_CHECK(request.signatureOk(metadata.name, uri));
    if (!request.signatureExists())
        std::cout << "warning: Unsigned request" << std::endl;

    Spend spend(*session.wallet);
    ABC_CHECK(spend.addPaymentRequest(&request));
    std::cout << "Sending " << request.amount() << " satoshis to "
              << metadata.name << std::endl;

    DataChunk rawTx;
    std::string txid;
    ABC_CHECK(spend.signTx(rawTx));
    ABC_CHECK(spend.broadcastTx(rawTx));
    ABC_CHECK(spend.saveTx(rawTx, txid));
    std::cout << "Transaction id: " << txid << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, SpendTransfer, "spend-transfer",
        " <wallet-dest> <amount>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto dest = argv[0];
    const auto amount = atol(argv[1]);

    std::shared_ptr<Wallet> target;
    ABC_CHECK(Wallet::create(target, *session.account, dest));
    Metadata metadata;
    metadata.name = session.wallet->name();

    Spend spend(*session.wallet);
    ABC_CHECK(spend.addTransfer(*target, amount, metadata));
    std::cout << "Sending " << amount << " satoshis to " << target->name()
              << std::endl;
    metadata.name = target->name();
    ABC_CHECK(spend.metadataSet(metadata));

    DataChunk rawTx;
    std::string txid;
    ABC_CHECK(spend.signTx(rawTx));
    ABC_CHECK(spend.broadcastTx(rawTx));
    ABC_CHECK(spend.saveTx(rawTx, txid));
    std::cout << "Transaction id: " << txid << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, SpendGetFee, "spend-get-fee",
        " <address> <amount>")
{
    if (argc != 2)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));
    const auto address = argv[0];
    const auto amount = atol(argv[1]);

    Spend spend(*session.wallet);
    ABC_CHECK(spend.addAddress(address, amount));

    uint64_t fee;
    ABC_CHECK(spend.calculateFees(fee));
    std::cout << "fee: " << fee << std::endl;

    return Status();
}

COMMAND(InitLevel::wallet, SpendGetMax, "spend-get-max",
        "")
{
    if (argc != 0)
        return ABC_ERROR(ABC_CC_Error, helpString(*this));

    Spend spend(*session.wallet);
    ABC_CHECK(spend.addAddress("1111111111111111111114oLvT2", 0));

    uint64_t max;
    ABC_CHECK(spend.calculateMax(max));
    std::cout << "max: " << max << std::endl;

    return Status();
}
