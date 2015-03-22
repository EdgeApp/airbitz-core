/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "JsonBox.hpp"
#include "../crypto/Crypto.hpp"

namespace abcd {

Status
JsonBox::encrypt(DataSlice data, DataSlice key)
{
    json_t *root;
    ABC_CHECK_OLD(ABC_CryptoEncryptJSONObject(toU08Buf(data),
        toU08Buf(key), ABC_CryptoType_AES256,
        &root, &error));
    reset(root);
    return Status();
}

Status
JsonBox::decrypt(DataChunk &result, DataSlice key)
{
    AutoU08Buf data;
    if (!root_)
        return ABC_ERROR(ABC_CC_DecryptError, "No encrypted data");
    ABC_CHECK_OLD(ABC_CryptoDecryptJSONObject(root_,
        toU08Buf(key), &data, &error));
    result = DataChunk(data.p, data.end);
    return Status();
}

} // namespace abcd
