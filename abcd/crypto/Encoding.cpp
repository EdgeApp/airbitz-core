/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Encoding.hpp"
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <algorithm>

namespace abcd {

static
int ABC_CryptoCalcBase64DecodeLength(const char *szDataBase64);

std::string
base32Encode(DataSlice data)
{
    const char base32Sym[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    std::string out;
    auto chunks = (data.size() + 4) / 5; // Rounding up
    out.reserve(8 * chunks);

    auto i = data.begin();
    uint16_t buffer = 0; // Bits waiting to be written out, MSB first
    int bits = 0; // Number of bits currently in the buffer
    while (i != data.end() || 0 < bits)
    {
        // Reload the buffer if we need more bits:
        if (i != data.end() && bits < 5)
        {
            buffer |= *i++ << (8 - bits);
            bits += 8;
        }

        // Write out 5 most-significant bits in the buffer:
        out += base32Sym[buffer >> 11];
        buffer <<= 5;
        bits -= 5;
    }

    // Pad the final string to a multiple of 8 characters long:
    out.append(-out.size() % 8, '=');
    return out;
}

bool
base32Decode(DataChunk &result, const std::string &in)
{
    // The string must be a multiple of 8 characters long:
    if (in.size() % 8)
        return false;

    DataChunk out;
    out.reserve(5 * (in.size() / 8));

    auto i = in.begin();
    uint16_t buffer = 0; // Bits waiting to be written out, MSB first
    int bits = 0; // Number of bits currently in the buffer
    while (i != in.end())
    {
        // Read one character from the string:
        int value = 0;
        if ('A' <= *i && *i <= 'Z')
            value = *i++ - 'A';
        else if ('2' <= *i && *i <= '7')
            value = 26 + *i++ - '2';
        else
            break;

        // Append the bits to the buffer:
        buffer |= value << (11 - bits);
        bits += 5;

        // Write out some bits if the buffer has a byte's worth:
        if (8 <= bits)
        {
            out.push_back(buffer >> 8);
            buffer <<= 8;
            bits -= 8;
        }
    }

    // Any extra characters must be '=':
    if (!std::all_of(i, in.end(), [](char c){ return '=' == c; }))
        return false;

    // There cannot be extra padding:
    if (8 <= in.end() - i)
        return false;

    // Any extra bits must be 0 (but rfc4648 decoders can be liberal here):
//    if (buffer != 0)
//        return false;

    result = std::move(out);
    return true;
}

/**
 * Encodes the data into a hex string
 *
 * @param pszDataHex Location to store allocated string (caller must free)
 */
tABC_CC ABC_CryptoHexEncode(const tABC_U08Buf Data,
                            char              **pszDataHex,
                            tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    unsigned int dataLength;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL(pszDataHex);

    char *szDataHex;
    dataLength = ABC_BUF_SIZE(Data);
    ABC_STR_NEW(szDataHex, (dataLength * 2) + 1);

    for (unsigned i = 0; i < dataLength; i++)
    {
        unsigned char *pCurByte = (unsigned char *) (ABC_BUF_PTR(Data) + i);
        sprintf(&(szDataHex[i * 2]), "%02x", *pCurByte);
    }
    *pszDataHex = szDataHex;

exit:

    return cc;
}

/**
 * Decodes the given hex string into data
 */
tABC_CC ABC_CryptoHexDecode(const char  *szDataHex,
                            tABC_U08Buf *pData,
                            tABC_Error  *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    unsigned int dataLength;

    ABC_CHECK_NULL(szDataHex);
    ABC_CHECK_NULL(pData);

    dataLength = (unsigned int) (strlen(szDataHex) / 2);

    ABC_BUF_NEW(*pData, dataLength);

    for (unsigned i = 0; i < dataLength; i++)
    {
        unsigned int val;
        sscanf(&(szDataHex[i * 2]), "%02x", &val);
        unsigned char *pCurByte = (unsigned char *) (ABC_BUF_PTR(*pData) + i);
        *pCurByte = (unsigned char) val;
    }

exit:

    return cc;
}

/**
 * Converts a buffer of binary data to a base-64 string.
 * @param Data The passed-in data. The caller owns this.
 * @param pszDataBase64 The returned string, which the caller must free().
 */
tABC_CC ABC_CryptoBase64Encode(const tABC_U08Buf Data,
                               char              **pszDataBase64,
                               tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    BIO *bio = NULL;
    BIO *mem = NULL;
    BIO *b64 = NULL;
    unsigned len;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL(pszDataBase64);

    // Set up the pipleline:
    bio = BIO_new(BIO_s_mem());
    ABC_CHECK_SYS(bio, "BIO_new_mem_buf");
    mem = bio;
    b64 = BIO_new(BIO_f_base64());
    ABC_CHECK_SYS(b64, "BIO_f_base64");
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer
    bio = BIO_push(b64, bio);

    // Push the data through:
    len = BIO_write(bio, ABC_BUF_PTR(Data), ABC_BUF_SIZE(Data));
    if (len != ABC_BUF_SIZE(Data))
    {
        ABC_RET_ERROR(ABC_CC_SysError, "Base64 encode has failed");
    }
    (void)BIO_flush(bio);

    // Move the data to the output:
    BUF_MEM *bptr;
    BIO_get_mem_ptr(mem, &bptr);
    ABC_STR_NEW(*pszDataBase64, bptr->length + 1);
    memcpy(*pszDataBase64, bptr->data, bptr->length);

exit:
    if (bio) BIO_free_all(bio);
    return cc;
}

/**
 * Converts a string of base-64 encoded data to a buffer of binary data.
 * @param pData An un-allocated data buffer. This function will allocate
 * memory and assign it to the buffer. The data should then be freed.
 */
tABC_CC ABC_CryptoBase64Decode(const char   *szDataBase64,
                               tABC_U08Buf  *pData,
                               tABC_Error   *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    BIO *bio = NULL;
    BIO *b64 = NULL;
    unsigned char *pTmpData = NULL;
    int len;
    int decodeLen;

    ABC_CHECK_NULL(szDataBase64);
    ABC_CHECK_NULL(pData);

    // Set up the pipeline:
    bio = BIO_new_mem_buf((void *)szDataBase64, (int)strlen(szDataBase64));
    ABC_CHECK_SYS(bio, "BIO_new_mem_buf");
    b64 = BIO_new(BIO_f_base64());
    ABC_CHECK_SYS(b64, "BIO_f_base64");
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer
    bio = BIO_push(b64, bio);

    // Push the data through:
    decodeLen = ABC_CryptoCalcBase64DecodeLength(szDataBase64);
    ABC_ARRAY_NEW(pTmpData, decodeLen + 1, unsigned char);
    len = BIO_read(bio, pTmpData, (int)strlen(szDataBase64));
    if (len != decodeLen)
    {
        ABC_RET_ERROR(ABC_CC_SysError, "Base64 decode is incorrect");
    }
    ABC_BUF_SET_PTR(*pData, pTmpData, len);

exit:
    if (bio) BIO_free_all(bio);
    return cc;
}

/*
 * Calculates the length of a decoded base64 string
 */
static
int ABC_CryptoCalcBase64DecodeLength(const char *szDataBase64)
{
    int len = (int) strlen(szDataBase64);
    int padding = 0;

    if (szDataBase64[len-1] == '=' && szDataBase64[len-2] == '=') //last two chars are =
    padding = 2;
    else if (szDataBase64[len-1] == '=') //last char is =
    padding = 1;

    return (3*len)/4 - padding;
}

} // namespace abcd
