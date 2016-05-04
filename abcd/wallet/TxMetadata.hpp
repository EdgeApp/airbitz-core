/*
 * Copyright (c) 2015, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_WALLET_TX_METADATA_HPP
#define ABCD_WALLET_TX_METADATA_HPP

#include "../util/Status.hpp"

namespace abcd {

class JsonObject;

/**
 * Common user-editable metadata for transactions and addresses.
 */
struct TxMetadata
{
    TxMetadata();
    TxMetadata(const tABC_TxDetails *pDetails);

    // User-editable metadata:
    std::string name;
    std::string category;
    std::string notes;
    unsigned bizId;
    double amountCurrency;

    /**
     * Loads the structure from a JSON object.
     */
    Status
    load(const JsonObject &json);

    /**
     * Writes the structure fields into the provided JSON object.
     */
    Status
    save(JsonObject &json) const;

    /**
     * Converts this structure to the legacy format.
     */
    tABC_TxDetails *
    toDetails() const;
};

} // namespace abcd

#endif
