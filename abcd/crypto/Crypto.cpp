/**
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 */

#include "Crypto.hpp"
#include "Encoding.hpp"
#include "Random.hpp"
#include "../json/JsonFile.hpp"
#include "../util/Util.hpp"
#include <bitcoin/bitcoin.hpp> // wow! such slow, very compile time
#include <openssl/evp.h>
#include <openssl/err.h>
#include <openssl/sha.h>

namespace abcd {

#define JSON_ENC_TYPE_FIELD     "encryptionType"
#define JSON_ENC_IV_FIELD       "iv_hex"
#define JSON_ENC_DATA_FIELD     "data_base64"

static
tABC_CC ABC_CryptoEncryptAES256Package(const tABC_U08Buf Data,
                                       const tABC_U08Buf Key,
                                       tABC_U08Buf       *pEncData,
                                       DataChunk         &IV,
                                       tABC_Error        *pError);
static
tABC_CC ABC_CryptoDecryptAES256Package(const tABC_U08Buf EncData,
                                       const tABC_U08Buf Key,
                                       const tABC_U08Buf IV,
                                       tABC_U08Buf       *pData,
                                       tABC_Error        *pError);
static
tABC_CC ABC_CryptoEncryptAES256(const tABC_U08Buf Data,
                                const tABC_U08Buf Key,
                                const tABC_U08Buf IV,
                                tABC_U08Buf       *pEncData,
                                tABC_Error        *pError);
static
tABC_CC ABC_CryptoDecryptAES256(const tABC_U08Buf EncData,
                                const tABC_U08Buf Key,
                                const tABC_U08Buf IV,
                                tABC_U08Buf       *pData,
                                tABC_Error        *pError);

std::string
cryptoFilename(DataSlice key, const std::string &name)
{
    return bc::encode_base58(bc::to_data_chunk(
        bc::hmac_sha256_hash(DataSlice(name), key)));
}

/**
 * Encrypt data into a jansson object
 */
tABC_CC ABC_CryptoEncryptJSONObject(const tABC_U08Buf Data,
                                    const tABC_U08Buf Key,
                                    tABC_CryptoType   cryptoType,
                                    json_t            **ppJSON_Enc,
                                    tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoU08Buf     EncData;
    DataChunk      IV;
    json_t          *jsonRoot       = NULL;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_ASSERT(cryptoType < ABC_CryptoType_Count, ABC_CC_UnknownCryptoType, "Invalid encryption type");
    ABC_CHECK_NULL(ppJSON_Enc);

    if (cryptoType == ABC_CryptoType_AES256)
    {
        // encrypt
        ABC_CHECK_RET(ABC_CryptoEncryptAES256Package(Data,
                                                     Key,
                                                     &EncData,
                                                     IV,
                                                     pError));

        // Encoding
        jsonRoot = json_pack("{sissss}",
            JSON_ENC_TYPE_FIELD, cryptoType,
            JSON_ENC_IV_FIELD,   base16Encode(IV).c_str(),
            JSON_ENC_DATA_FIELD, base64Encode(U08Buf(EncData)).c_str());

        // assign our final result
        *ppJSON_Enc = jsonRoot;
        json_incref(jsonRoot);  // because we will decl below
    }
    else
    {
        ABC_RET_ERROR(ABC_CC_InvalidCryptoType, "Unsupported encryption type");
    }

exit:
    if (jsonRoot)     json_decref(jsonRoot);

    return cc;
}

/**
 * Encrypted the given data and write the json to a file
 */
tABC_CC ABC_CryptoEncryptJSONFile(const tABC_U08Buf Data,
                                  const tABC_U08Buf Key,
                                  tABC_CryptoType   cryptoType,
                                  const char *szFilename,
                                  tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    json_t *root = nullptr;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(szFilename);

    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(Data, Key, cryptoType, &root, pError));
    ABC_CHECK_NEW(JsonFile(root).save(szFilename), pError);

exit:
    return cc;
}

/**
 * Encrypted the given json and write the encrypted json to a file
 */
tABC_CC ABC_CryptoEncryptJSONFileObject(json_t *pJSON_Data,
                                        const tABC_U08Buf Key,
                                        tABC_CryptoType  cryptoType,
                                        const char *szFilename,
                                        tABC_Error  *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    std::string data;

    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL(pJSON_Data);

    ABC_CHECK_NEW(JsonFile(json_incref(pJSON_Data)).encode(data), pError);
     // Downstream decoders often forget to null-terminate their input.
     // This is a bug, but we can save the app from crashing by
     // including a null byte in the encrypted data.
    data.push_back(0);
    ABC_CHECK_RET(ABC_CryptoEncryptJSONFile(toU08Buf(data), Key, cryptoType, szFilename, pError));

exit:
    return cc;
}

/**
 * Given a JSON object holding encrypted data, this function decrypts it
 */
tABC_CC ABC_CryptoDecryptJSONObject(const json_t      *pJSON_Enc,
                                    const tABC_U08Buf Key,
                                    tABC_U08Buf       *pData,
                                    tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    DataChunk data;
    DataChunk iv;
    int type;
    json_t *jsonVal = NULL;

    ABC_CHECK_NULL(pJSON_Enc);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pData);

    jsonVal = json_object_get(pJSON_Enc, JSON_ENC_TYPE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_number(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON encrypt package - missing type");
    type = (int) json_integer_value(jsonVal);
    ABC_CHECK_ASSERT(ABC_CryptoType_AES256 == type, ABC_CC_UnknownCryptoType, "Invalid encryption type");

    // get the IV
    jsonVal = json_object_get(pJSON_Enc, JSON_ENC_IV_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON encrypt package - missing iv");
    ABC_CHECK_NEW(base16Decode(iv, json_string_value(jsonVal)), pError);

    // get the encrypted data
    jsonVal = json_object_get(pJSON_Enc, JSON_ENC_DATA_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON encrypt package - missing data");
    ABC_CHECK_NEW(base64Decode(data, json_string_value(jsonVal)), pError);

    // decrypted the data
    ABC_CHECK_RET(ABC_CryptoDecryptAES256Package(toU08Buf(data), Key, toU08Buf(iv), pData, pError));

exit:
    return cc;
}

/**
 * Given a file holding encrypted data, this function decrypts it
 */
tABC_CC ABC_CryptoDecryptJSONFile(const char *szFilename,
                                  const tABC_U08Buf Key,
                                  tABC_U08Buf       *pData,
                                  tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    JsonFile json;

    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pData);

    ABC_CHECK_NEW(json.load(szFilename), pError);
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(json.root(), Key, pData, pError));

exit:
    return cc;
}

/**
 * Loads the given file, decrypts it and creates the json object from it
 *
 * @param ppJSON_Data pointer to store allocated json object
 *                   (the user is responsible for json_decref'ing)
 */
tABC_CC ABC_CryptoDecryptJSONFileObject(const char *szFilename,
                                        const tABC_U08Buf Key,
                                        json_t **ppJSON_Data,
                                        tABC_Error  *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoU08Buf Data;
    JsonFile file;

    ABC_CHECK_NULL(szFilename);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(ppJSON_Data);
    *ppJSON_Data = NULL;

    ABC_CHECK_RET(ABC_CryptoDecryptJSONFile(szFilename, Key, &Data, pError));
    ABC_CHECK_NEW(file.decode(toString(U08Buf(Data))), pError);
    *ppJSON_Data = json_incref(file.root());

exit:
    return cc;
}

/**
 * Creates an encrypted aes256 package that includes data, random header/footer and sha256
 * Package format:
 *   1 byte:     h (the number of random header bytes)
 *   h bytes:    h random header bytes
 *   4 bytes:    length of data (big endian)
 *   x bytes:    data (x bytes)
 *   1 byte:     f (the number of random footer bytes)
 *   f bytes:    f random header bytes
 *   32 bytes:   32 bytes SHA256 of all data up to this point
 */
static
tABC_CC ABC_CryptoEncryptAES256Package(const tABC_U08Buf Data,
                                       const tABC_U08Buf Key,
                                       tABC_U08Buf       *pEncData,
                                       DataChunk         &IV,
                                       tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    DataChunk headerData;
    DataChunk footerData;
    AutoU08Buf UnencryptedData;
    unsigned char nRandomHeaderBytes;
    unsigned char nRandomFooterBytes;
    unsigned long totalSizeUnencrypted = 0;
    unsigned char *pCurUnencryptedData = NULL;
    unsigned char nSizeByte = 0;
    unsigned char sha256Output[SHA256_DIGEST_LENGTH];

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pEncData);

    // create a random IV
    ABC_CHECK_NEW(randomData(IV, AES_256_IV_LENGTH), pError);

    // create a random number of header bytes 0-255
    {
        DataChunk r;
        ABC_CHECK_NEW(randomData(r, 1), pError);
        nRandomHeaderBytes = r[0];
    }
    //printf("rand header count: %d\n", nRandomHeaderBytes);
    ABC_CHECK_NEW(randomData(headerData, nRandomHeaderBytes), pError);

    // create a random number of footer bytes 0-255
    {
        DataChunk r;
        ABC_CHECK_NEW(randomData(r, 1), pError);
        nRandomFooterBytes = r[0];
    }
    //printf("rand footer count: %d\n", nRandomFooterBytes);
    ABC_CHECK_NEW(randomData(footerData, nRandomFooterBytes), pError);

    // calculate the size of our unencrypted buffer
    totalSizeUnencrypted += 1; // header count
    totalSizeUnencrypted += nRandomHeaderBytes; // header
    totalSizeUnencrypted += 4; // space to hold data size
    totalSizeUnencrypted += ABC_BUF_SIZE(Data); // data
    totalSizeUnencrypted += 1; // footer count
    totalSizeUnencrypted += nRandomFooterBytes; // footer
    totalSizeUnencrypted += SHA256_DIGEST_LENGTH; // sha256
    //printf("total size unencrypted: %lu\n", (unsigned long) totalSizeUnencrypted);

    // allocate the unencrypted buffer
    ABC_BUF_NEW(UnencryptedData, totalSizeUnencrypted);
    pCurUnencryptedData = ABC_BUF_PTR(UnencryptedData);

    // add the random header count and bytes
    memcpy(pCurUnencryptedData, &nRandomHeaderBytes, 1);
    pCurUnencryptedData += 1;
    memcpy(pCurUnencryptedData, headerData.data(), headerData.size());
    pCurUnencryptedData += nRandomHeaderBytes;

    // add the size of the data
    nSizeByte = (ABC_BUF_SIZE(Data) >> 24) & 0xff;
    memcpy(pCurUnencryptedData, &nSizeByte, 1);
    pCurUnencryptedData += 1;
    nSizeByte = (ABC_BUF_SIZE(Data) >> 16) & 0xff;
    memcpy(pCurUnencryptedData, &nSizeByte, 1);
    pCurUnencryptedData += 1;
    nSizeByte = (ABC_BUF_SIZE(Data) >> 8) & 0xff;
    memcpy(pCurUnencryptedData, &nSizeByte, 1);
    pCurUnencryptedData += 1;
    nSizeByte = (ABC_BUF_SIZE(Data) >> 0) & 0xff;
    memcpy(pCurUnencryptedData, &nSizeByte, 1);
    pCurUnencryptedData += 1;

    // add the data
    memcpy(pCurUnencryptedData, ABC_BUF_PTR(Data), ABC_BUF_SIZE(Data));
    pCurUnencryptedData += ABC_BUF_SIZE(Data);

    // add the random footer count and bytes
    memcpy(pCurUnencryptedData, &nRandomFooterBytes, 1);
    pCurUnencryptedData += 1;
    memcpy(pCurUnencryptedData, footerData.data(), footerData.size());
    pCurUnencryptedData += nRandomFooterBytes;

    // add the sha256
    SHA256(ABC_BUF_PTR(UnencryptedData), totalSizeUnencrypted - SHA256_DIGEST_LENGTH, sha256Output);
    memcpy(pCurUnencryptedData, sha256Output, SHA256_DIGEST_LENGTH);
    pCurUnencryptedData += SHA256_DIGEST_LENGTH;

    // encrypted our new unencrypted package
    ABC_CHECK_RET(ABC_CryptoEncryptAES256(UnencryptedData, Key, toU08Buf(IV), pEncData, pError));

exit:
    return cc;
}

/**
 * Decrypts an encrypted aes256 package which includes data, random header/footer and sha256
 * Note: it is critical that this function returns ABC_CC_DecryptFailure if there is an issue
 *       because code is counting on this specific error to know a key is bad
 * Package format:
 *   1 byte:     h (the number of random header bytes)
 *   h bytes:    h random header bytes
 *   4 bytes:    length of data (big endian)
 *   x bytes:    data (x bytes)
 *   1 byte:     f (the number of random footer bytes)
 *   f bytes:    f random header bytes
 *   32 bytes:   32 bytes SHA256 of all data up to this point
 */
static
tABC_CC ABC_CryptoDecryptAES256Package(const tABC_U08Buf EncData,
                                       const tABC_U08Buf Key,
                                       const tABC_U08Buf IV,
                                       tABC_U08Buf       *pData,
                                       tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoU08Buf Data;
    unsigned char headerLength;
    unsigned int minSize;
    unsigned char *pDataLengthPos;
    unsigned int dataSecLength;
    unsigned char footerLength;
    unsigned int shaCheckLength;
    unsigned char *pSHALoc;
    unsigned char sha256Output[SHA256_DIGEST_LENGTH];
    unsigned char *pFinalDataPos;

    ABC_CHECK_NULL_BUF(EncData);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL_BUF(IV);
    ABC_CHECK_NULL(pData);

    // start by decrypting the pacakge
    if (ABC_CC_Ok != ABC_CryptoDecryptAES256(EncData, Key, IV, &Data, pError))
    {
        cc = ABC_CC_DecryptFailure;
        if (pError)
        {
            pError->code = cc;
        }
        goto exit;
    }

    // get the size of the random header section
    headerLength = *ABC_BUF_PTR(Data);

    // check that we have enough data based upon this info
    minSize = 1 + headerLength + 4 + 1 + 1 + SHA256_DIGEST_LENGTH; // decrypted package must be at least this big
    ABC_CHECK_ASSERT(ABC_BUF_SIZE(Data) >= minSize, ABC_CC_DecryptFailure, "Decrypted data is not long enough");

    // get the size of the data section
    pDataLengthPos = ABC_BUF_PTR(Data) + (1 + headerLength);
    dataSecLength = ((unsigned int) *pDataLengthPos) << 24;
    pDataLengthPos++;
    dataSecLength += ((unsigned int) *pDataLengthPos) << 16;
    pDataLengthPos++;
    dataSecLength += ((unsigned int) *pDataLengthPos) << 8;
    pDataLengthPos++;
    dataSecLength += ((unsigned int) *pDataLengthPos);

    // check that we have enough data based upon this info
    minSize = 1 + headerLength + 4 + dataSecLength + 1 + SHA256_DIGEST_LENGTH; // decrypted package must be at least this big
    ABC_CHECK_ASSERT(ABC_BUF_SIZE(Data) >= minSize, ABC_CC_DecryptFailure, "Decrypted data is not long enough");

    // get the size of the random footer section
    footerLength = *(ABC_BUF_PTR(Data) + 1 + headerLength + 4 + dataSecLength);

    // check that we have enough data based upon this info
    minSize = 1 + headerLength + 4 + dataSecLength + 1 + footerLength + SHA256_DIGEST_LENGTH; // decrypted package must be at least this big
    ABC_CHECK_ASSERT(ABC_BUF_SIZE(Data) >= minSize, ABC_CC_DecryptFailure, "Decrypted data is not long enough");

    // set up for the SHA check
    shaCheckLength = 1 + headerLength + 4 + dataSecLength + 1 + footerLength; // all but the sha
    pSHALoc = ABC_BUF_PTR(Data) + shaCheckLength;

    // calc the sha256
    SHA256(ABC_BUF_PTR(Data), shaCheckLength, sha256Output);

    // check the sha256
    if (0 != memcmp(pSHALoc, sha256Output, SHA256_DIGEST_LENGTH))
    {
        // this can be specifically used by the caller to possibly determine whether the key was incorrect
        ABC_RET_ERROR(ABC_CC_DecryptFailure, "Decrypted data failed checksum (SHA) check");
    }

    // all is good, so create the final data
    pFinalDataPos = ABC_BUF_PTR(Data) + 1 + headerLength + 4;
    ABC_BUF_NEW(*pData, dataSecLength);
    memcpy(ABC_BUF_PTR(*pData), pFinalDataPos, dataSecLength);

exit:
    return cc;
}

/**
 * Encrypts the given data with AES256
 */
static
tABC_CC ABC_CryptoEncryptAES256(const tABC_U08Buf Data,
                                const tABC_U08Buf Key,
                                const tABC_U08Buf IV,
                                tABC_U08Buf       *pEncData,
                                tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    unsigned char aKey[AES_256_KEY_LENGTH];
    unsigned int keyLength = ABC_BUF_SIZE(Key);
    unsigned char aIV[AES_256_IV_LENGTH];
    unsigned int IVLength;
    int c_len;
    int f_len = 0;
    unsigned char *pTmpEncData = NULL;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL_BUF(IV);
    ABC_CHECK_NULL(pEncData);

    // create the final key
    memset(aKey, 0, AES_256_KEY_LENGTH);
    if (keyLength > AES_256_KEY_LENGTH)
    {
        keyLength = AES_256_KEY_LENGTH;
    }
    memcpy(aKey, ABC_BUF_PTR(Key), keyLength);

    // create the IV
    memset(aIV, 0, AES_256_IV_LENGTH);
    IVLength = ABC_BUF_SIZE(IV);
    if (IVLength > AES_256_IV_LENGTH)
    {
        IVLength = AES_256_IV_LENGTH;
    }
    memcpy(aIV, ABC_BUF_PTR(IV), IVLength);

    // init our cipher text struct
    EVP_CIPHER_CTX e_ctx;
    EVP_CIPHER_CTX_init(&e_ctx);
    EVP_EncryptInit_ex(&e_ctx, EVP_aes_256_cbc(), NULL, aKey, aIV);

    // max ciphertext len for a n bytes of plaintext is n + AES_256_BLOCK_LENGTH -1 bytes
    c_len = ABC_BUF_SIZE(Data) + AES_256_BLOCK_LENGTH;
    ABC_ARRAY_NEW(pTmpEncData, c_len, unsigned char);

    // allows reusing of 'e' for multiple encryption cycles
    EVP_EncryptInit_ex(&e_ctx, NULL, NULL, NULL, NULL);

    // update pTmpEncData, c_len is filled with the length of pTmpEncData generated, dataLength is the size of plaintext in bytes
    EVP_EncryptUpdate(&e_ctx, pTmpEncData, &c_len, ABC_BUF_PTR(Data), ABC_BUF_SIZE(Data));

    // update pTmpEncData with the final remaining bytes
    EVP_EncryptFinal_ex(&e_ctx, pTmpEncData + c_len, &f_len);

    // set final values
    ABC_BUF_SET_PTR(*pEncData, pTmpEncData, c_len + f_len);

    EVP_CIPHER_CTX_cleanup(&e_ctx);

exit:

    return cc;
}

/**
 * Decrypts the given data with AES256
 */
static
tABC_CC ABC_CryptoDecryptAES256(const tABC_U08Buf EncData,
                                const tABC_U08Buf Key,
                                const tABC_U08Buf IV,
                                tABC_U08Buf       *pData,
                                tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    unsigned char *pTmpData = NULL;
    int p_len, f_len;
    unsigned int IVLength;
    unsigned int keyLength;

    ABC_CHECK_NULL_BUF(EncData);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL_BUF(IV);
    ABC_CHECK_NULL(pData);

    // create the final key
    unsigned char aKey[AES_256_KEY_LENGTH];
    memset(aKey, 0, AES_256_KEY_LENGTH);
    keyLength = ABC_BUF_SIZE(Key);
    if (keyLength > AES_256_KEY_LENGTH)
    {
        keyLength = AES_256_KEY_LENGTH;
    }
    memcpy(aKey, ABC_BUF_PTR(Key), keyLength);

    // create the IV
    unsigned char aIV[AES_256_IV_LENGTH];
    memset(aIV, 0, AES_256_IV_LENGTH);
    IVLength = ABC_BUF_SIZE(IV);
    if (IVLength > AES_256_IV_LENGTH)
    {
        IVLength = AES_256_IV_LENGTH;
    }
    memcpy(aIV, ABC_BUF_PTR(IV), IVLength);

    // init our cipher text struct
    EVP_CIPHER_CTX d_ctx;
    EVP_CIPHER_CTX_init(&d_ctx);
    EVP_DecryptInit_ex(&d_ctx, EVP_aes_256_cbc(), NULL, aKey, aIV);

    /* because we have padding ON, we must allocate an extra cipher block size of memory */
    p_len = ABC_BUF_SIZE(EncData);
    f_len = 0;
    ABC_ARRAY_NEW(pTmpData, p_len + AES_256_BLOCK_LENGTH, unsigned char);

    // decrypt
    EVP_DecryptInit_ex(&d_ctx, NULL, NULL, NULL, NULL);
    EVP_DecryptUpdate(&d_ctx, pTmpData, &p_len, ABC_BUF_PTR(EncData), ABC_BUF_SIZE(EncData));
    EVP_DecryptFinal_ex(&d_ctx, pTmpData + p_len, &f_len);

    EVP_CIPHER_CTX_cleanup(&d_ctx);

    // set final values
    ABC_BUF_SET_PTR(*pData, pTmpData, p_len + f_len);

exit:
    return cc;
}

} // namespace abcd
