/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_CRYPTO_ENCODING_HPP
#define ABCD_CRYPTO_ENCODING_HPP

#include "../util/Data.hpp"
#include "../util/Status.hpp"

namespace abcd {

/**
 * Encodes data into a base-32 string according to rfc4648.
 */
std::string
base32Encode(DataSlice data);

/**
 * Decodes a base-32 string as defined by rfc4648.
 */
Status
base32Decode(DataChunk &result, const std::string &in);

/**
 * Encodes data into a hex string.
 */
std::string
base64Encode(DataSlice data);

/**
 * Decodes a hex string.
 */
Status
base64Decode(DataChunk &result, const std::string &in);

// Old versions:
tABC_CC ABC_CryptoHexEncode(const tABC_U08Buf Data,
                            char              **pszDataHex,
                            tABC_Error        *pError);

tABC_CC ABC_CryptoHexDecode(const char  *szDataHex,
                            tABC_U08Buf *pData,
                            tABC_Error  *pError);

} // namespace abcd

#endif
