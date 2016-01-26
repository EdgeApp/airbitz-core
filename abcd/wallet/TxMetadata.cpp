/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "TxMetadata.hpp"
#include "../util/Util.hpp"

namespace abcd {

TxMetadata::TxMetadata():
    bizId(0),
    amountCurrency(0),
    amountSatoshi(0),
    amountFeesAirbitzSatoshi(0),
    amountFeesMinersSatoshi(0)
{
}

TxMetadata::TxMetadata(const tABC_TxDetails *pDetails):
    name(pDetails->szName ? pDetails->szName : ""),
    category(pDetails->szCategory ? pDetails->szCategory : ""),
    notes(pDetails->szNotes ? pDetails->szNotes : ""),
    bizId(pDetails->bizId),
    amountCurrency (pDetails->amountCurrency),
    amountSatoshi(pDetails->amountSatoshi),
    amountFeesAirbitzSatoshi(pDetails->amountFeesAirbitzSatoshi),
    amountFeesMinersSatoshi(pDetails->amountFeesMinersSatoshi)
{
}

tABC_TxDetails *
TxMetadata::toDetails() const
{
    tABC_TxDetails *out = structAlloc<tABC_TxDetails>();
    out->szName = stringCopy(name);
    out->szCategory = stringCopy(category);
    out->szNotes = stringCopy(notes);
    out->bizId = bizId;
    out->amountCurrency  = amountCurrency;
    out->amountSatoshi = amountSatoshi;
    out->amountFeesAirbitzSatoshi = amountFeesAirbitzSatoshi;
    out->amountFeesMinersSatoshi = amountFeesMinersSatoshi;
    out->attributes = 0;
    return out;
}

} // namespace abcd
