/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_JSON_JSON_BOX_HPP
#define ABCD_JSON_JSON_BOX_HPP

#include "JsonPtr.hpp"
#include "../util/Data.hpp"

namespace abcd {

/**
 * A json object holding encrypted data.
 */
class JsonBox:
    public JsonPtr
{
public:
    ABC_JSON_CONSTRUCTORS(JsonBox, JsonPtr)

    /**
     * Puts a value into the box, encrypting it with the given key.
     */
    Status
    encrypt(DataSlice data, DataSlice key);

    /**
     * Extracts the value from the box, decrypting it with the given key.
     */
    Status
    decrypt(DataChunk &result, DataSlice key);
};

} // namespace abcd

#endif
