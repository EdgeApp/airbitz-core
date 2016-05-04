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

    // Obsolete/moved fields:
    ABC_JSON_INTEGER(attributes,        "attributes", 0);
    ABC_JSON_INTEGER(amount,            "amountSatoshi", 0);
    ABC_JSON_INTEGER(minerFee,          "amountFeeMinersSatoshi", 0);
    ABC_JSON_INTEGER(airbitzFee,        "amountFeeAirBitzSatoshi", 0);
};

TxMetadata::TxMetadata():
    bizId(0),
    amountCurrency(0)
{
}

TxMetadata::TxMetadata(const tABC_TxDetails *pDetails):
    name(pDetails->szName ? pDetails->szName : ""),
    category(pDetails->szCategory ? pDetails->szCategory : ""),
    notes(pDetails->szNotes ? pDetails->szNotes : ""),
    bizId(pDetails->bizId),
    amountCurrency (pDetails->amountCurrency)
{
}

Status
TxMetadata::load(const JsonObject &json)
{
    MetadataJson metaJson(json);
    name           = metaJson.name();
    category       = metaJson.category();
    notes          = metaJson.notes();
    bizId          = metaJson.bizId();
    amountCurrency = metaJson.amountCurrency();
    return Status();
}

Status
TxMetadata::save(JsonObject &json) const
{
    MetadataJson metaJson(json);
    ABC_CHECK(metaJson.nameSet(name));
    ABC_CHECK(metaJson.categorySet(category));
    ABC_CHECK(metaJson.notesSet(notes));
    ABC_CHECK(metaJson.bizIdSet(bizId));
    ABC_CHECK(metaJson.amountCurrencySet(amountCurrency));

    // Obsolete/moved fields:
    ABC_CHECK(metaJson.attributesSet(0));
    ABC_CHECK(metaJson.amountSet(0));
    ABC_CHECK(metaJson.minerFeeSet(0));
    ABC_CHECK(metaJson.airbitzFeeSet(0));
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
    out->amountSatoshi = 0;
    out->amountFeesAirbitzSatoshi = 0;
    out->amountFeesMinersSatoshi = 0;
    return out;
}

} // namespace abcd
