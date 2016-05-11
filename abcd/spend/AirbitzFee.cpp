/*
 * Copyright (c) 2016, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "AirbitzFee.hpp"
#include "Spend.hpp"
#include "../General.hpp"
#include "../wallet/Wallet.hpp"

namespace abcd {

int64_t
airbitzFeeOutgoing(const AirbitzFeeInfo &info, int64_t spent)
{
    if (spent <= 0)
        return 0;
    int64_t fee = info.outgoingRate * spent;
    if (fee < info.noFeeMinSatoshi)
        return 0;
    if (fee < info.outgoingMin)
        fee = info.outgoingMin;
    if (info.outgoingMax < fee)
        fee = info.outgoingMax;
    return fee;
}

int64_t
airbitzFeeIncoming(const AirbitzFeeInfo &info, int64_t received)
{
    if (received <= 0)
        return 0;
    int64_t fee = info.incomingRate * received;
    if (fee < info.incomingMin)
        fee = info.incomingMin;
    if (info.incomingMax < fee)
        fee = info.incomingMax;
    return fee;
}

Status
airbitzFeeAutoSend(Wallet &wallet)
{
    const auto info = generalAirbitzFeeInfo();
    if (info.addresses.empty())
        return Status();

    const auto owed = wallet.txs.airbitzFeePending();
    if (owed < info.sendMin)
        return Status();

    const auto last = wallet.txs.airbitzFeeLastSent();
    if (time(nullptr) < last + info.sendPeriod)
        return Status();

    Metadata metadata;
    metadata.name = info.sendPayee;
    metadata.category = info.sendCategory;

    Spend spend(wallet);
    ABC_CHECK(spend.metadataSet(metadata));
    DataChunk rawTx;
    ABC_CHECK(spend.signTx(rawTx));
    ABC_CHECK(spend.broadcastTx(rawTx));
    std::string txid;
    ABC_CHECK(spend.saveTx(rawTx, txid));

    return Status();
}

} // namespace abcd
