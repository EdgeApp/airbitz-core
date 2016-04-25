/*
 * Copyright (c) 2014, Airbitz, Inc.
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
    ABC_CHECK_OLD(ABC_CryptoEncryptJSONObject(data,
                  key, ABC_CryptoType_AES256,
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
                  key, &data, &error));
    result = DataChunk(data.begin(), data.end());
    return Status();
}

} // namespace abcd
