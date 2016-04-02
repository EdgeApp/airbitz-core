/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "TxMetadata.hpp"
#include "../json/JsonObject.hpp"
#include "../util/Util.hpp"

namespace abcd {

struct MetadataJson:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(MetadataJson, JsonObject);
    ABC_JSON_STRING(name,               "name", "");
    ABC_JSON_STRING(category,           "category", "");
    ABC_JSON_STRING(notes,              "notes", "");
    ABC_JSON_INTEGER(bizId,             "bizId", 0);
    ABC_JSON_NUMBER(amountCurrency,     "amountCurrency", 0);
    ABC_JSON_INTEGER(amount,            "amountSatoshi", 0);
    ABC_JSON_INTEGER(airbitzFee,        "amountFeeAirBitzSatoshi", 0);
    ABC_JSON_INTEGER(minerFee,          "amountFeeMinersSatoshi", 0);

    // Obsolete fields:
    ABC_JSON_INTEGER(attributes,        "attributes", 0);
};

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

Status
TxMetadata::load(JsonPtr json)
{
    MetadataJson metaJson(json);
    name           = metaJson.name();
    category       = metaJson.category();
    notes          = metaJson.notes();
    bizId          = metaJson.bizId();
    amountCurrency = metaJson.amountCurrency();
    amountSatoshi  = metaJson.amount();
    amountFeesAirbitzSatoshi = metaJson.airbitzFee();
    amountFeesMinersSatoshi  = metaJson.minerFee();
    return Status();
}

Status
TxMetadata::save(JsonPtr &result) const
{
    MetadataJson out;
    ABC_CHECK(out.nameSet(name));
    ABC_CHECK(out.categorySet(category));
    ABC_CHECK(out.notesSet(notes));
    ABC_CHECK(out.bizIdSet(bizId));
    ABC_CHECK(out.amountCurrencySet(amountCurrency));
    ABC_CHECK(out.amountSet(amountSatoshi));
    ABC_CHECK(out.airbitzFeeSet(amountFeesAirbitzSatoshi));
    ABC_CHECK(out.minerFeeSet(amountFeesMinersSatoshi));

    // Obsolete fields:
    ABC_CHECK(out.attributesSet(0));

    result = out;
    return Status();
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
