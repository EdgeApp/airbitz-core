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
#include "../json/JsonFile.hpp"
#include "../util/Data.hpp"
#include "../util/FileIO.hpp"
#include "../util/Util.hpp"
#include <unistd.h>
#include <sys/time.h>
#ifndef __ANDROID__
#include <sys/statvfs.h>
#endif
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/hmac.h>
#include "../../minilibs/scrypt/sha256.h"

namespace abcd {

#define JSON_ENC_TYPE_FIELD     "encryptionType"
#define JSON_ENC_IV_FIELD       "iv_hex"
#define JSON_ENC_DATA_FIELD     "data_base64"

#define UUID_BYTE_COUNT         16
#define UUID_STR_LENGTH         (UUID_BYTE_COUNT * 2) + 4

static
tABC_CC ABC_CryptoEncryptAES256Package(const tABC_U08Buf Data,
                                       const tABC_U08Buf Key,
                                       tABC_U08Buf       *pEncData,
                                       tABC_U08Buf       *pIV,
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
static
int ABC_CryptoCalcBase64DecodeLength(const char *szDataBase64);

/**
 * Sets the seed for the random number generator
 */
tABC_CC ABC_CryptoSetRandomSeed(const tABC_U08Buf Seed,
                                tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char *szFileIORootDir = NULL;
    unsigned long timeVal;
    time_t timeResult;
    clock_t clockVal;
    pid_t pid;
    std::string rootDir = getRootDir();

    AutoU08Buf NewSeed;

    ABC_CHECK_NULL_BUF(Seed);

    // create our own copy so we can add to it
    ABC_BUF_DUP(NewSeed, Seed);

    // mix in some info on our file system
#ifndef __ANDROID__
    ABC_BUF_APPEND_PTR(NewSeed, rootDir.data(), rootDir.size());
    struct statvfs fiData;
    if ((statvfs(rootDir.c_str(), &fiData)) >= 0 )
    {
        ABC_BUF_APPEND_PTR(NewSeed, (unsigned char *)&fiData, sizeof(struct statvfs));

        //printf("Disk %s: \n", szRootDir);
        //printf("\tblock size: %lu\n", fiData.f_bsize);
        //printf("\ttotal no blocks: %i\n", fiData.f_blocks);
        //printf("\tfree blocks: %i\n", fiData.f_bfree);
    }
#endif

    // add some time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    timeVal = tv.tv_sec * tv.tv_usec;
    ABC_BUF_APPEND_PTR(NewSeed, &timeVal, sizeof(unsigned long));

    timeResult = time(NULL);
    ABC_BUF_APPEND_PTR(NewSeed, &timeResult, sizeof(time_t));

    clockVal = clock();
    ABC_BUF_APPEND_PTR(NewSeed, &clockVal, sizeof(clock_t));

    timeVal = CLOCKS_PER_SEC ;
    ABC_BUF_APPEND_PTR(NewSeed, &timeVal, sizeof(unsigned long));

    // add process id's
    pid = getpid();
    ABC_BUF_APPEND_PTR(NewSeed, &pid, sizeof(pid_t));

    pid = getppid();
    ABC_BUF_APPEND_PTR(NewSeed, &pid, sizeof(pid_t));

    // TODO: add more random seed data here

    // seed it
    RAND_seed(ABC_BUF_PTR(NewSeed), ABC_BUF_SIZE(NewSeed));

exit:
    ABC_FREE_STR(szFileIORootDir);

    return cc;
}

/**
 * Creates a buffer of random data
 */
tABC_CC ABC_CryptoCreateRandomData(unsigned int  length,
                                   tABC_U08Buf   *pData,
                                   tABC_Error    *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    int rc;

    ABC_CHECK_NULL(pData);

    ABC_BUF_NEW(*pData, length);

    rc = RAND_bytes(ABC_BUF_PTR(*pData), length);
    //unsigned long err = ERR_get_error();

    if (rc != 1)
    {
        ABC_BUF_FREE(*pData);
        ABC_RET_ERROR(ABC_CC_Error, "Random data generation failed");
    }

exit:

    return cc;
}

/**
 * generates a randome UUID (version 4)
 *
 * Version 4 UUIDs use a scheme relying only on random numbers.
 * This algorithm sets the version number (4 bits) as well as two reserved bits.
 * All other bits (the remaining 122 bits) are set using a random or pseudorandom data source.
 * Version 4 UUIDs have the form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx where x is any hexadecimal
 * digit and y is one of 8, 9, A, or B (e.g., F47AC10B-58CC-4372-A567-0E02B2E3D479).
 */
tABC_CC ABC_CryptoGenUUIDString(char       **pszUUID,
                                tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    unsigned char *pData = NULL;
    char *szUUID = NULL;
    AutoU08Buf Data;

    ABC_CHECK_NULL(pszUUID);

    ABC_STR_NEW(szUUID, UUID_STR_LENGTH + 1);

    ABC_CHECK_RET(ABC_CryptoCreateRandomData(UUID_BYTE_COUNT, &Data, pError));
    pData = ABC_BUF_PTR(Data);

    // put in the version
    // To put in the version, take the 7th byte and perform an and operation using 0x0f, followed by an or operation with 0x40.
    pData[6] = (pData[6] & 0xf) | 0x40;

    // 9th byte significant nibble is one of 8, 9, A, or B
    pData[8] = (pData[8] | 0x80) & 0xbf;

    sprintf(szUUID, "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
            pData[0], pData[1], pData[2], pData[3], pData[4], pData[5], pData[6], pData[7],
            pData[8], pData[9], pData[10], pData[11], pData[12], pData[13], pData[14], pData[15]);

    *pszUUID = szUUID;

exit:
    return cc;
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
    AutoU08Buf     IV;
    char            *szIV_Hex       = NULL;
    char            *szDataBase64   = NULL;
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
                                                     &IV,
                                                     pError));

        // encode the IV into a Hex string
        ABC_CHECK_RET(ABC_CryptoHexEncode(IV, &szIV_Hex, pError));

        // encode the encrypted data into base64
        ABC_CHECK_RET(ABC_CryptoBase64Encode(EncData, &szDataBase64, pError));

        // Encoding
        jsonRoot = json_pack("{sissss}",
                            JSON_ENC_TYPE_FIELD, cryptoType,
                            JSON_ENC_IV_FIELD,   szIV_Hex,
                            JSON_ENC_DATA_FIELD, szDataBase64);

        // assign our final result
        *ppJSON_Enc = jsonRoot;
        json_incref(jsonRoot);  // because we will decl below
    }
    else
    {
        ABC_RET_ERROR(ABC_CC_InvalidCryptoType, "Unsupported encryption type");
    }

exit:
    ABC_FREE_STR(szIV_Hex);
    ABC_FREE_STR(szDataBase64);
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

    AutoU08Buf     EncData;
    AutoU08Buf     IV;

    int type;
    json_t *jsonVal = NULL;
    const char *szIV = NULL;
    const char *szDataBase64 = NULL;

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
    szIV = json_string_value(jsonVal);
    ABC_CHECK_RET(ABC_CryptoHexDecode(szIV, &IV, pError));

    // get the encrypted data
    jsonVal = json_object_get(pJSON_Enc, JSON_ENC_DATA_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON encrypt package - missing data");
    szDataBase64 = json_string_value(jsonVal);
    ABC_CHECK_RET(ABC_CryptoBase64Decode(szDataBase64, &EncData, pError));

    // decrypted the data
    ABC_CHECK_RET(ABC_CryptoDecryptAES256Package(EncData, Key, IV, pData, pError));

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
                                       tABC_U08Buf       *pIV,
                                       tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoU08Buf RandHeaderBytes;
    AutoU08Buf RandFooterBytes;
    AutoU08Buf UnencryptedData;
    unsigned char nRandomHeaderBytes;
    unsigned char nRandomFooterBytes;
    unsigned long totalSizeUnencrypted = 0;
    unsigned char *pCurUnencryptedData = NULL;
    unsigned char nSizeByte = 0;
    unsigned char sha256Output[SHA_256_LENGTH];

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pEncData);
    ABC_CHECK_NULL(pIV);

    // create a random IV
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(AES_256_IV_LENGTH, pIV, pError));

    // create a random number of header bytes 0-255
    {
        AutoU08Buf RandCount;
        ABC_CHECK_RET(ABC_CryptoCreateRandomData(1, &RandCount, pError));
        nRandomHeaderBytes = *(RandCount.p);
    }
    //printf("rand header count: %d\n", nRandomHeaderBytes);
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(nRandomHeaderBytes, &RandHeaderBytes, pError));

    // create a random number of footer bytes 0-255
    {
        AutoU08Buf RandCount;
        ABC_CHECK_RET(ABC_CryptoCreateRandomData(1, &RandCount, pError));
        nRandomFooterBytes = *(RandCount.p);
    }
    //printf("rand footer count: %d\n", nRandomFooterBytes);
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(nRandomFooterBytes, &RandFooterBytes, pError));

    // calculate the size of our unencrypted buffer
    totalSizeUnencrypted += 1; // header count
    totalSizeUnencrypted += nRandomHeaderBytes; // header
    totalSizeUnencrypted += 4; // space to hold data size
    totalSizeUnencrypted += ABC_BUF_SIZE(Data); // data
    totalSizeUnencrypted += 1; // footer count
    totalSizeUnencrypted += nRandomFooterBytes; // footer
    totalSizeUnencrypted += SHA_256_LENGTH; // sha256
    //printf("total size unencrypted: %lu\n", (unsigned long) totalSizeUnencrypted);

    // allocate the unencrypted buffer
    ABC_BUF_NEW(UnencryptedData, totalSizeUnencrypted);
    pCurUnencryptedData = ABC_BUF_PTR(UnencryptedData);

    // add the random header count and bytes
    memcpy(pCurUnencryptedData, &nRandomHeaderBytes, 1);
    pCurUnencryptedData += 1;
    memcpy(pCurUnencryptedData, ABC_BUF_PTR(RandHeaderBytes), ABC_BUF_SIZE(RandHeaderBytes));
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
    memcpy(pCurUnencryptedData, ABC_BUF_PTR(RandFooterBytes), ABC_BUF_SIZE(RandFooterBytes));
    pCurUnencryptedData += nRandomFooterBytes;

    // add the sha256
    SHA256_CTX sha256Context;
    sc_SHA256_Init(&sha256Context);
    sc_SHA256_Update(&sha256Context, ABC_BUF_PTR(UnencryptedData), totalSizeUnencrypted - SHA_256_LENGTH);
    sc_SHA256_Final(sha256Output, &sha256Context);
    memcpy(pCurUnencryptedData, sha256Output, SHA_256_LENGTH);
    pCurUnencryptedData += SHA_256_LENGTH;

    // encrypted our new unencrypted package
    ABC_CHECK_RET(ABC_CryptoEncryptAES256(UnencryptedData, Key, *pIV, pEncData, pError));

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
    SHA256_CTX sha256Context;
    unsigned char sha256Output[SHA_256_LENGTH];
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
    minSize = 1 + headerLength + 4 + 1 + 1 + SHA_256_LENGTH; // decrypted package must be at least this big
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
    minSize = 1 + headerLength + 4 + dataSecLength + 1 + SHA_256_LENGTH; // decrypted package must be at least this big
    ABC_CHECK_ASSERT(ABC_BUF_SIZE(Data) >= minSize, ABC_CC_DecryptFailure, "Decrypted data is not long enough");

    // get the size of the random footer section
    footerLength = *(ABC_BUF_PTR(Data) + 1 + headerLength + 4 + dataSecLength);

    // check that we have enough data based upon this info
    minSize = 1 + headerLength + 4 + dataSecLength + 1 + footerLength + SHA_256_LENGTH; // decrypted package must be at least this big
    ABC_CHECK_ASSERT(ABC_BUF_SIZE(Data) >= minSize, ABC_CC_DecryptFailure, "Decrypted data is not long enough");

    // set up for the SHA check
    shaCheckLength = 1 + headerLength + 4 + dataSecLength + 1 + footerLength; // all but the sha
    pSHALoc = ABC_BUF_PTR(Data) + shaCheckLength;

    // calc the sha256
    sc_SHA256_Init(&sha256Context);
    sc_SHA256_Update(&sha256Context, ABC_BUF_PTR(Data), shaCheckLength);
    sc_SHA256_Final(sha256Output, &sha256Context);

    // check the sha256
    if (0 != memcmp(pSHALoc, sha256Output, SHA_256_LENGTH))
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

/**
 * Generates HMAC-256 of the given data with the given key
 * @param pDataHMAC An un-allocated data buffer. This function will allocate
 * memory and assign it to the buffer. The data should then be freed.
 */
tABC_CC ABC_CryptoHMAC256(tABC_U08Buf Data,
                          tABC_U08Buf Key,
                          tABC_U08Buf *pDataHMAC,
                          tABC_Error  *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pDataHMAC);

    // call HMAC-256 in openssl
    ABC_BUF_NEW(*pDataHMAC, HMAC_SHA_256_LENGTH);
    HMAC(EVP_sha256(), ABC_BUF_PTR(Key), ABC_BUF_SIZE(Key), ABC_BUF_PTR(Data), ABC_BUF_SIZE(Data), ABC_BUF_PTR(*pDataHMAC), NULL);

exit:

    return cc;
}

/**
 * Generates HMAC-512 of the given data with the given key
 * @param pDataHMAC An un-allocated data buffer. This function will allocate
 * memory and assign it to the buffer. The data should then be freed.
 */
tABC_CC ABC_CryptoHMAC512(tABC_U08Buf Data,
                          tABC_U08Buf Key,
                          tABC_U08Buf *pDataHMAC,
                          tABC_Error  *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pDataHMAC);

    // call HMAC-512 in openssl
    ABC_BUF_NEW(*pDataHMAC, HMAC_SHA_512_LENGTH);
    HMAC(EVP_sha512(), ABC_BUF_PTR(Key), ABC_BUF_SIZE(Key), ABC_BUF_PTR(Data), ABC_BUF_SIZE(Data), ABC_BUF_PTR(*pDataHMAC), NULL);

exit:

    return cc;
}

} // namespace abcd
