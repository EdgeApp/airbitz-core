/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "JsonBox.hpp"
#include "../crypto/Crypto.hpp"
#include "../crypto/Encoding.hpp"
#include <sodium.h>

namespace abcd {

/**
 * Initializes libsodium.
 */
struct SodiumInit
{
    Status status;

    SodiumInit()
    {
        if (sodium_init() < 0)
            status = ABC_ERROR(ABC_CC_SysError, "Cannot initialize libsodium");
    }
};
SodiumInit sodiumInit;

enum CryptoType
{
    AES256_CBC_AIRBITZ = 0,
    CHACHA20_POLY1305_IETF = 1
};

Status
JsonBox::encrypt(DataSlice data, DataSlice key)
{
    DataChunk nonce;
    AutoU08Buf cyphertext;
    ABC_CHECK_OLD(ABC_CryptoEncryptAES256Package(data, key,
                  &cyphertext, nonce, &error));

    ABC_CHECK(typeSet(AES256_CBC_AIRBITZ));
    ABC_CHECK(nonceSet(base16Encode(nonce)));
    ABC_CHECK(cyphertextSet(base64Encode(cyphertext)));

    return Status();
}

Status
JsonBox::decrypt(DataChunk &result, DataSlice key)
{
    DataChunk nonce;
    ABC_CHECK(nonceOk());
    ABC_CHECK(base16Decode(nonce, this->nonce()));

    DataChunk cyphertext;
    ABC_CHECK(cyphertextOk());
    ABC_CHECK(base64Decode(cyphertext, this->cyphertext()));

    switch (type())
    {
    case AES256_CBC_AIRBITZ:
    {
        ABC_CHECK_OLD(ABC_CryptoDecryptAES256Package(result,
                      cyphertext, key, nonce,
                      &error));
        return Status();
    }

    case CHACHA20_POLY1305_IETF:
    {
        DataChunk data(cyphertext.size() -
                       crypto_aead_chacha20poly1305_IETF_ABYTES);
        unsigned long long dataSize;

        if (crypto_aead_chacha20poly1305_IETF_NPUBBYTES != nonce.size())
            return ABC_ERROR(ABC_CC_DecryptError, "Bad nonce size");
        if (crypto_aead_chacha20poly1305_IETF_KEYBYTES != key.size())
            return ABC_ERROR(ABC_CC_DecryptError, "Bad key size");

        ABC_CHECK(sodiumInit.status);
        if (0 != crypto_aead_chacha20poly1305_ietf_decrypt(
                    data.data(), &dataSize, nullptr,
                    cyphertext.data(), cyphertext.size(),
                    nullptr, 0, nonce.data(), key.data()))
            return ABC_ERROR(ABC_CC_DecryptError, "Invalid data");

        data.resize(dataSize);
        result = std::move(data);
        return Status();
    }

    default:
        return ABC_ERROR(ABC_CC_DecryptError, "Unknown encryption type");
    }
}

} // namespace abcd
