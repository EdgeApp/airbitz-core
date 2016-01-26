/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * AirBitz cryptographic function wrappers.
 */

#ifndef ABCD_CRYPTO_CRYPTO_HPP
#define ABCD_CRYPTO_CRYPTO_HPP

#include "../util/Data.hpp"
#include "../../src/ABC.h"
#include <jansson.h>

namespace abcd {

#define AES_256_IV_LENGTH       16
#define AES_256_BLOCK_LENGTH    16
#define AES_256_KEY_LENGTH      32

typedef enum eABC_CryptoType
{
    ABC_CryptoType_AES256 = 0,
    ABC_CryptoType_Count
} tABC_CryptoType;

/**
 * Creates a cryptographically secure filename from a meaningful name
 * and a secret key.
 * This prevents the filename from leaking information about its contents
 * to anybody but the key holder.
 */
std::string
cryptoFilename(DataSlice key, const std::string &name);

// Encryption:
tABC_CC ABC_CryptoEncryptJSONObject(const tABC_U08Buf Data,
                                    const tABC_U08Buf Key,
                                    tABC_CryptoType   cryptoType,
                                    json_t            **ppJSON_Enc,
                                    tABC_Error        *pError);

tABC_CC ABC_CryptoEncryptJSONFile(const tABC_U08Buf Data,
                                  const tABC_U08Buf Key,
                                  tABC_CryptoType   cryptoType,
                                  const char *szFilename,
                                  tABC_Error        *pError);

tABC_CC ABC_CryptoEncryptJSONFileObject(json_t *pJSON_Data,
                                        const tABC_U08Buf Key,
                                        tABC_CryptoType cryptoType,
                                        const char *szFilename,
                                        tABC_Error  *pError);

tABC_CC ABC_CryptoDecryptJSONObject(const json_t      *pJSON_Enc,
                                    const tABC_U08Buf Key,
                                    tABC_U08Buf       *pData,
                                    tABC_Error        *pError);

tABC_CC ABC_CryptoDecryptJSONFile(const char *szFilename,
                                  const tABC_U08Buf Key,
                                  tABC_U08Buf       *pData,
                                  tABC_Error        *pError);

tABC_CC ABC_CryptoDecryptJSONFileObject(const char *szFilename,
                                        const tABC_U08Buf Key,
                                        json_t **ppJSON_Data,
                                        tABC_Error  *pError);

} // namespace abcd

#endif
