/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_CRYPTO_SCRYPT_HPP
#define ABCD_CRYPTO_SCRYPT_HPP

#include "../json/JsonObject.hpp"
#include "../util/Data.hpp"
#include "../util/Status.hpp"

namespace abcd {

constexpr size_t scryptDefaultSize = 32;

/*
 * Initializes Scrypt parameters by benchmarking the device.
 */
tABC_CC ABC_InitializeCrypto(tABC_Error *pError);

/**
 * Parameters for the scrypt algorithm.
 */
struct ScryptSnrp
{
    DataChunk salt;
    uint64_t n;
    uint32_t r;
    uint32_t p;

    /**
     * Initializes the parameters with a random salt and
     * benchmarked difficulty settings.
     */
    Status
    create();

    /**
     * The scrypt hash function.
     */
    Status
    hash(DataChunk &result, DataSlice data, size_t size=scryptDefaultSize) const;
};

/**
 * Returns the fixed SNRP value used for the username.
 */
const ScryptSnrp &
usernameSnrp();

/**
 * Serialization format.
 */
struct JsonSnrp:
    public JsonObject
{
    ABC_JSON_CONSTRUCTORS(JsonSnrp, JsonObject)

    ABC_JSON_STRING(salt, "salt_hex", nullptr)
    ABC_JSON_INTEGER(n, "n", 0)
    ABC_JSON_INTEGER(r, "r", 0)
    ABC_JSON_INTEGER(p, "p", 0)

    Status
    snrpGet(ScryptSnrp &result) const;

    Status
    snrpSet(const ScryptSnrp &snrp);

    Status
    create();

    Status
    hash(DataChunk &result, DataSlice data) const;
};

} // namespace abcd

#endif
