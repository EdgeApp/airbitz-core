/**
 * @file
 * AirBitz cryptographic function prototypes
 *
 * This file contains all of the functions associated with cryptography.
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/statvfs.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/err.h>
#include <jansson.h>
#include "crypto_scrypt.h"
#include "sha256.h"
#include "ABC.h"
#include "ABC_Crypto.h"
#include "ABC_Util.h"
#include "ABC_FileIO.h"

#define JSON_ENC_TYPE_FIELD     "encryptionType"
#define JSON_ENC_SALT_FIELD     "salt_hex"
#define JSON_ENC_N_FIELD        "n"
#define JSON_ENC_R_FIELD        "r"
#define JSON_ENC_P_FIELD        "p"
#define JSON_ENC_IV_FIELD       "iv_hex"
#define JSON_ENC_DATA_FIELD     "data_base64"
#define JSON_ENC_SNRP_FIELD     "SNRP"

#define UUID_BYTE_COUNT         16
#define UUID_STR_LENGTH         (UUID_BYTE_COUNT * 2) + 4

#define SCRYPT_DEFAULT_SERVER_N    16384    // can't change as server uses this as well
#define SCRYPT_DEFAULT_SERVER_R    1        // can't change as server uses this as well
#define SCRYPT_DEFAULT_SERVER_P    1        // can't change as server uses this as well
#define SCRYPT_DEFAULT_CLIENT_N    16384
#define SCRYPT_DEFAULT_CLIENT_R    1
#define SCRYPT_DEFAULT_CLIENT_P    1
#define SCRYPT_DEFAULT_LENGTH      32
#define SCRYPT_DEFAULT_SALT_LENGTH 32


static unsigned char gaS1[] = { 0xb5, 0x86, 0x5f, 0xfb, 0x9f, 0xa7, 0xb3, 0xbf, 0xe4, 0xb2, 0x38, 0x4d, 0x47, 0xce, 0x83, 0x1e, 0xe2, 0x2a, 0x4a, 0x9d, 0x5c, 0x34, 0xc7, 0xef, 0x7d, 0x21, 0x46, 0x7c, 0xc7, 0x58, 0xf8, 0x1b };

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

// sets the seed for the random number generator
tABC_CC ABC_CryptoSetRandomSeed(const tABC_U08Buf Seed,
                                tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf NewSeed = ABC_BUF_NULL;

    ABC_CHECK_NULL_BUF(Seed);

    // create our own copy so we can add to it
    ABC_BUF_DUP(NewSeed, Seed);

    // mix in some info on our file system
    const char *szRootDir;
    ABC_CHECK_RET(ABC_FileIOGetRootDir(&szRootDir, pError));
    struct statvfs fiData;
    if ((statvfs(szRootDir, &fiData)) >= 0 ) 
    {
        ABC_BUF_APPEND_PTR(NewSeed, (unsigned char *)&fiData, sizeof(struct statvfs));
        
        //printf("Disk %s: \n", szRootDir);
        //printf("\tblock size: %lu\n", fiData.f_bsize);
        //printf("\ttotal no blocks: %i\n", fiData.f_blocks);
        //printf("\tfree blocks: %i\n", fiData.f_bfree);
    }

    // add some time
    struct timeval tv;
    gettimeofday(&tv, NULL);
    unsigned long timeVal = tv.tv_sec * tv.tv_usec;
    ABC_BUF_APPEND_PTR(NewSeed, &timeVal, sizeof(unsigned long));

    time_t timeResult = time(NULL);
    ABC_BUF_APPEND_PTR(NewSeed, &timeResult, sizeof(time_t));

    clock_t clockVal = clock();
    ABC_BUF_APPEND_PTR(NewSeed, &clockVal, sizeof(clock_t));

    timeVal = CLOCKS_PER_SEC ;
    ABC_BUF_APPEND_PTR(NewSeed, &timeVal, sizeof(unsigned long));

    // add process id's
    pid_t pid = getpid();
    ABC_BUF_APPEND_PTR(NewSeed, &pid, sizeof(pid_t));

    pid = getppid();
    ABC_BUF_APPEND_PTR(NewSeed, &pid, sizeof(pid_t));

    // TODO: add more random seed data here

    //ABC_UtilHexDumpBuf("Random Seed", NewSeed);

    // seed it
    RAND_seed(ABC_BUF_PTR(NewSeed), ABC_BUF_SIZE(NewSeed));

exit:

    ABC_BUF_FREE(NewSeed);

    return cc;
}

tABC_CC ABC_CryptoEncryptJSONString(const tABC_U08Buf Data,
                                    const tABC_U08Buf Key,
                                    tABC_CryptoType   cryptoType,
                                    char              **pszEncDataJSON,
                                    tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t          *jsonRoot       = NULL;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pszEncDataJSON);

    ABC_CHECK_RET(ABC_CryptoEncryptJSONObject(Data, Key, cryptoType, &jsonRoot, pError));
    *pszEncDataJSON = json_dumps(jsonRoot, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    
exit:
    if (jsonRoot)     json_decref(jsonRoot);

    return cc;
}

// encrypt data into a jansson object
tABC_CC ABC_CryptoEncryptJSONObject(const tABC_U08Buf Data,
                                    const tABC_U08Buf Key,
                                    tABC_CryptoType   cryptoType,
                                    json_t            **ppJSON_Enc,
                                    tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf     GenKey          = ABC_BUF_NULL;
    tABC_U08Buf     Salt            = ABC_BUF_NULL;
    tABC_U08Buf     EncData         = ABC_BUF_NULL;
    tABC_U08Buf     IV              = ABC_BUF_NULL;
    char            *szSalt_Hex     = NULL;
    char            *szIV_Hex       = NULL;
    char            *szDataBase64   = NULL;
    json_t          *jsonRoot       = NULL;
    json_t          *jsonSNRP       = NULL;
    tABC_CryptoSNRP *pSNRP          = NULL;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_ASSERT(cryptoType < ABC_CryptoType_Count, ABC_CC_UnknownCryptoType, "Invalid encryption type");
    ABC_CHECK_NULL(ppJSON_Enc);
    
    if ((cryptoType == ABC_CryptoType_AES256) || (cryptoType == ABC_CryptoType_AES256_Scrypt))
    {
        const tABC_U08Buf *pFinalKey = &Key;

        // if we are using scrypt then we need to gen the key
        if (cryptoType == ABC_CryptoType_AES256_Scrypt)
        {
            // gen some salt
            ABC_CHECK_RET(ABC_CryptoCreateRandomData(SCRYPT_DEFAULT_SALT_LENGTH, &Salt, pError));

            // encode the Salt into a Hex string
            ABC_CHECK_RET(ABC_CryptoHexEncode(Salt, &szSalt_Hex, pError));

            // generate a key using the salt and scrypt
            ABC_CHECK_RET(ABC_CryptoScrypt(Key,
                                           Salt,
                                           SCRYPT_DEFAULT_CLIENT_N,
                                           SCRYPT_DEFAULT_CLIENT_R,
                                           SCRYPT_DEFAULT_CLIENT_P,
                                           AES_256_KEY_LENGTH,
                                           &GenKey,
                                           pError));
            pFinalKey = &GenKey;
        }
        
        // encrypt
        ABC_CHECK_RET(ABC_CryptoEncryptAES256Package(Data,
                                                     *pFinalKey,
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

        // if this is Scrypt version, we need to add SNRP
        if (cryptoType == ABC_CryptoType_AES256_Scrypt)
        {
            ABC_CHECK_RET(ABC_CryptoCreateSNRP(Salt,
                                               SCRYPT_DEFAULT_CLIENT_N,
                                               SCRYPT_DEFAULT_CLIENT_R,
                                               SCRYPT_DEFAULT_CLIENT_P,
                                               &pSNRP,
                                               pError));
            ABC_CHECK_RET(ABC_CryptoCreateJSONObjectSNRP(pSNRP, &jsonSNRP, pError));
            json_object_set(jsonRoot, JSON_ENC_SNRP_FIELD, jsonSNRP);
        }
        
        // assign our final result
        *ppJSON_Enc = jsonRoot;
        json_incref(jsonRoot);  // because we will decl below
    }
    else 
    {
        ABC_RET_ERROR(ABC_CC_InvalidCryptoType, "Unsupported encryption type");
    }

exit:
    ABC_BUF_FREE(GenKey);
    ABC_BUF_FREE(Salt);
    ABC_BUF_FREE(EncData);
    ABC_BUF_FREE(IV);
    if (szSalt_Hex)   free(szSalt_Hex);
    if (szIV_Hex)     free(szIV_Hex);
    if (szDataBase64) free(szDataBase64);
    if (jsonRoot)     json_decref(jsonRoot);
    if (pSNRP)        ABC_CryptoFreeSNRP(&pSNRP);

    return cc;
}

// given a JSON string holding encrypted data, this function decrypts it
tABC_CC ABC_CryptoDecryptJSONString(const char        *szEncDataJSON,
                                    const tABC_U08Buf Key,
                                    tABC_U08Buf       *pData,
                                    tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t          *root   = NULL;

    ABC_CHECK_NULL(szEncDataJSON);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pData);

    json_error_t error;
    root = json_loads(szEncDataJSON, 0, &error);
    ABC_CHECK_ASSERT(root != NULL, ABC_CC_DecryptError, "Error parsing JSON encrypt package");
    ABC_CHECK_ASSERT(json_is_object(root), ABC_CC_DecryptError, "Error parsing JSON encrypt package");

    // decrypted the object data
    ABC_CHECK_RET(ABC_CryptoDecryptJSONObject(root, Key, pData, pError));
   
exit:
    if (root) json_decref(root);
    
    return cc;
}

// given a JSON object holding encrypted data, this function decrypts it
tABC_CC ABC_CryptoDecryptJSONObject(const json_t      *pJSON_Enc,
                                    const tABC_U08Buf Key,
                                    tABC_U08Buf       *pData,
                                    tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf     EncData = ABC_BUF_NULL;
    tABC_U08Buf     IV      = ABC_BUF_NULL;
    tABC_U08Buf     GenKey  = ABC_BUF_NULL;
    tABC_U08Buf     Salt    = ABC_BUF_NULL;
    tABC_CryptoSNRP *pSNRP  = NULL;

    ABC_CHECK_NULL(pJSON_Enc);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pData);

    json_t *jsonVal = json_object_get(pJSON_Enc, JSON_ENC_TYPE_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_number(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON encrypt package - missing type");
    int type = (int) json_integer_value(jsonVal);
    ABC_CHECK_ASSERT((ABC_CryptoType_AES256 == type || ABC_CryptoType_AES256_Scrypt == type), ABC_CC_UnknownCryptoType, "Invalid encryption type");

    const tABC_U08Buf *pFinalKey = &Key;

    // check if we need to gen a new key
    if (ABC_CryptoType_AES256_Scrypt == type)
    {
        // Decode the SNRP
        json_t *jsonSNRP = json_object_get(pJSON_Enc, JSON_ENC_SNRP_FIELD);
        ABC_CHECK_ASSERT((jsonSNRP && json_is_object(jsonSNRP)), ABC_CC_DecryptError, "Error parsing JSON encrypt package - missing SNRP");
        ABC_CHECK_RET(ABC_CryptoDecodeJSONObjectSNRP(jsonSNRP, &pSNRP, pError));

        // generate a key using the salt and scrypt
        ABC_CHECK_RET(ABC_CryptoScryptSNRP(Key, pSNRP, &GenKey, pError));
        pFinalKey = &GenKey;
    }

    // get the IV
    jsonVal = json_object_get(pJSON_Enc, JSON_ENC_IV_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON encrypt package - missing iv");
    const char *szIV = json_string_value(jsonVal);
    ABC_CHECK_RET(ABC_CryptoHexDecode(szIV, &IV, pError));

    // get the encrypted data
    jsonVal = json_object_get(pJSON_Enc, JSON_ENC_DATA_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON encrypt package - missing data");
    const char *szDataBase64 = json_string_value(jsonVal);
    ABC_CHECK_RET(ABC_CryptoBase64Decode(szDataBase64, &EncData, pError));

    // decrypted the data
    ABC_CHECK_RET(ABC_CryptoDecryptAES256Package(EncData, *pFinalKey, IV, pData, pError));

exit:
    ABC_BUF_FREE(IV);
    ABC_BUF_FREE(EncData);
    ABC_BUF_FREE(GenKey);
    ABC_BUF_FREE(Salt);
    ABC_CryptoFreeSNRP(&pSNRP);
    
    return cc;
}

// creates an encrypted aes256 package that includes data, random header/footer and sha256
/* Package format:
    1 byte:     h (the number of random header bytes)
    h bytes:    h random header bytes
    4 bytes:    length of data (big endian)
    x bytes:    data (x bytes)
    1 byte:     f (the number of random footer bytes)
    f bytes:    f random header bytes
    32 bytes:   32 bytes SHA256 of all data up to this point
*/
static 
tABC_CC ABC_CryptoEncryptAES256Package(const tABC_U08Buf Data,
                                       const tABC_U08Buf Key,
                                       tABC_U08Buf       *pEncData,
                                       tABC_U08Buf       *pIV,
                                       tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf RandCount = ABC_BUF_NULL;
    tABC_U08Buf RandHeaderBytes = ABC_BUF_NULL;
    tABC_U08Buf RandFooterBytes = ABC_BUF_NULL;
    tABC_U08Buf UnencryptedData = ABC_BUF_NULL;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL(pEncData);
    ABC_CHECK_NULL(pIV);

    // create a random IV
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(AES_256_IV_LENGTH, pIV, pError));

    // create a random number of header bytes 0-255
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(1, &RandCount, pError));
    unsigned char nRandomHeaderBytes = *(RandCount.p);
    ABC_BUF_FREE(RandCount);
    //printf("rand header count: %d\n", nRandomHeaderBytes);
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(nRandomHeaderBytes, &RandHeaderBytes, pError));
    //ABC_UtilHexDumpBuf("Rand Header Bytes", RandHeaderBytes);

    // create a random number of footer bytes 0-255
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(1, &RandCount, pError));
    unsigned char nRandomFooterBytes = *(RandCount.p);
    ABC_BUF_FREE(RandCount);
    //printf("rand footer count: %d\n", nRandomFooterBytes);
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(nRandomFooterBytes, &RandFooterBytes, pError));
    //ABC_UtilHexDumpBuf("Rand Footer Bytes", RandFooterBytes);

    // calculate the size of our unencrypted buffer
    unsigned long totalSizeUnencrypted = 0;
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
    unsigned char *pCurUnencryptedData = ABC_BUF_PTR(UnencryptedData);

    // add the random header count and bytes
    memcpy(pCurUnencryptedData, &nRandomHeaderBytes, 1);
    pCurUnencryptedData += 1;
    memcpy(pCurUnencryptedData, ABC_BUF_PTR(RandHeaderBytes), ABC_BUF_SIZE(RandHeaderBytes));
    pCurUnencryptedData += nRandomHeaderBytes;

    // add the size of the data
    unsigned char nSizeByte = 0;
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
    unsigned char sha256Output[SHA_256_LENGTH];
    sc_SHA256_Init(&sha256Context);
    sc_SHA256_Update(&sha256Context, ABC_BUF_PTR(UnencryptedData), totalSizeUnencrypted - SHA_256_LENGTH);
    sc_SHA256_Final(sha256Output, &sha256Context);
    memcpy(pCurUnencryptedData, sha256Output, SHA_256_LENGTH);
    pCurUnencryptedData += SHA_256_LENGTH;
    //ABC_UtilHexDump("SHA_256", sha256Output,  SHA_256_LENGTH);

    //ABC_UtilHexDumpBuf("Unencrypted data", UnencryptedData);

    //ABC_UtilHexDumpBuf("IV", *pIV);

    //ABC_UtilHexDumpBuf("Key", Key);

    // encrypted our new unencrypted package
    ABC_CHECK_RET(ABC_CryptoEncryptAES256(UnencryptedData, Key, *pIV, pEncData, pError));

    //ABC_UtilHexDumpBuf("Encrypted data", *pEncData);

    
exit:
    ABC_BUF_FREE(RandCount);
    ABC_BUF_FREE(RandHeaderBytes);
    ABC_BUF_FREE(RandFooterBytes);
    ABC_BUF_FREE(UnencryptedData);

    return cc;
}

// decrypts an encrypted aes256 package which includes data, random header/footer and sha256
/* Package format:
    1 byte:     h (the number of random header bytes)
    h bytes:    h random header bytes
    4 bytes:    length of data (big endian)
    x bytes:    data (x bytes)
    1 byte:     f (the number of random footer bytes)
    f bytes:    f random header bytes
    32 bytes:   32 bytes SHA256 of all data up to this point
*/
static 
tABC_CC ABC_CryptoDecryptAES256Package(const tABC_U08Buf EncData,
                                       const tABC_U08Buf Key,
                                       const tABC_U08Buf IV,
                                       tABC_U08Buf       *pData,
                                       tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf Data = ABC_BUF_NULL;

    ABC_CHECK_NULL_BUF(EncData);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL_BUF(IV);
    ABC_CHECK_NULL(pData);

    // start by decrypting the pacakge
    ABC_CHECK_RET(ABC_CryptoDecryptAES256(EncData, Key, IV, &Data, pError));

    // get the size of the random header section
    unsigned char headerLength = *ABC_BUF_PTR(Data);

    // check that we have enough data based upon this info
    unsigned int minSize = 1 + headerLength + 4 + 1 + 1 + SHA_256_LENGTH; // decrypted package must be at least this big
    ABC_CHECK_ASSERT(ABC_BUF_SIZE(Data) >= minSize, ABC_CC_DecryptError, "Decrypted data is not long enough");

    // get the size of the data section
    unsigned char *pDataLengthPos = ABC_BUF_PTR(Data) + (1 + headerLength);
    unsigned int dataSecLength = ((unsigned int) *pDataLengthPos) << 24;
    pDataLengthPos++;
    dataSecLength += ((unsigned int) *pDataLengthPos) << 16;
    pDataLengthPos++;
    dataSecLength += ((unsigned int) *pDataLengthPos) << 8;
    pDataLengthPos++;
    dataSecLength += ((unsigned int) *pDataLengthPos);

    // check that we have enough data based upon this info
    minSize = 1 + headerLength + 4 + dataSecLength + 1 + SHA_256_LENGTH; // decrypted package must be at least this big
    ABC_CHECK_ASSERT(ABC_BUF_SIZE(Data) >= minSize, ABC_CC_DecryptError, "Decrypted data is not long enough");

    // get the size of the random footer section
    unsigned char footerLength = *(ABC_BUF_PTR(Data) + 1 + headerLength + 4 + dataSecLength);

    // check that we have enough data based upon this info
    minSize = 1 + headerLength + 4 + dataSecLength + 1 + footerLength + SHA_256_LENGTH; // decrypted package must be at least this big
    ABC_CHECK_ASSERT(ABC_BUF_SIZE(Data) >= minSize, ABC_CC_DecryptError, "Decrypted data is not long enough");

    // set up for the SHA check
    unsigned int shaCheckLength = 1 + headerLength + 4 + dataSecLength + 1 + footerLength; // all but the sha
    unsigned char *pSHALoc = ABC_BUF_PTR(Data) + shaCheckLength;

    // calc the sha256
    SHA256_CTX sha256Context;
    unsigned char sha256Output[SHA_256_LENGTH];
    sc_SHA256_Init(&sha256Context);
    sc_SHA256_Update(&sha256Context, ABC_BUF_PTR(Data), shaCheckLength);
    sc_SHA256_Final(sha256Output, &sha256Context);

    // check the sha256
    if (0 != memcmp(pSHALoc, sha256Output, SHA_256_LENGTH))
    {
        // this can be specifically used by the caller to possibly determine whether the key was incorrect
        ABC_RET_ERROR(ABC_CC_DecryptBadChecksum, "Decrypted data failed checksum (SHA) check");
    }

    // all is good, so create the final data
    unsigned char *pFinalDataPos = ABC_BUF_PTR(Data) + 1 + headerLength + 4;
    ABC_BUF_NEW(*pData, dataSecLength);
    memcpy(ABC_BUF_PTR(*pData), pFinalDataPos, dataSecLength);
    
exit:
    ABC_BUF_FREE(Data);

    return cc;
}

// encrypts the given data with AES256
static 
tABC_CC ABC_CryptoEncryptAES256(const tABC_U08Buf Data,
                                const tABC_U08Buf Key,
                                const tABC_U08Buf IV,
                                tABC_U08Buf       *pEncData,
                                tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL_BUF(IV);
    ABC_CHECK_NULL(pEncData);

    // create the final key
    unsigned char aKey[AES_256_KEY_LENGTH];
    memset(aKey, 0, AES_256_KEY_LENGTH);
    unsigned int keyLength = ABC_BUF_SIZE(Key);
    if (keyLength > AES_256_KEY_LENGTH)
    {
        keyLength = AES_256_KEY_LENGTH;
    }
    memcpy(aKey, ABC_BUF_PTR(Key), keyLength);

    // create the IV
    unsigned char aIV[AES_256_IV_LENGTH];
    memset(aIV, 0, AES_256_IV_LENGTH);
    unsigned int IVLength = ABC_BUF_SIZE(IV);
    if (IVLength > AES_256_IV_LENGTH)
    {
        IVLength = AES_256_IV_LENGTH;
    }
    memcpy(aIV, ABC_BUF_PTR(IV), IVLength);

    //ABC_UtilHexDump("Key", aKey, AES_256_KEY_LENGTH);
    //ABC_UtilHexDump("IV", aIV, AES_256_IV_LENGTH);

    // init our cipher text struct
    EVP_CIPHER_CTX e_ctx;
    EVP_CIPHER_CTX_init(&e_ctx);
    EVP_EncryptInit_ex(&e_ctx, EVP_aes_256_cbc(), NULL, aKey, aIV);

    // max ciphertext len for a n bytes of plaintext is n + AES_256_BLOCK_LENGTH -1 bytes
    int c_len = ABC_BUF_SIZE(Data) + AES_256_BLOCK_LENGTH;
    int f_len = 0;
    unsigned char *pTmpEncData = malloc(c_len);
    
    // allows reusing of 'e' for multiple encryption cycles
    EVP_EncryptInit_ex(&e_ctx, NULL, NULL, NULL, NULL);
    
    // update pTmpEncData, c_len is filled with the length of pTmpEncData generated, dataLength is the size of plaintext in bytes
    EVP_EncryptUpdate(&e_ctx, pTmpEncData, &c_len, ABC_BUF_PTR(Data), ABC_BUF_SIZE(Data));
    
    // update pTmpEncData with the final remaining bytes
    EVP_EncryptFinal_ex(&e_ctx, pTmpEncData + c_len, &f_len);
    
    // set final values
    ABC_BUF_SET_PTR(*pEncData, pTmpEncData, c_len + f_len);

exit:

    return cc;
}


// decrypts the given data with AES256
static
tABC_CC ABC_CryptoDecryptAES256(const tABC_U08Buf EncData,
                                const tABC_U08Buf Key,
                                const tABC_U08Buf IV,
                                tABC_U08Buf       *pData,
                                tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL_BUF(EncData);
    ABC_CHECK_NULL_BUF(Key);
    ABC_CHECK_NULL_BUF(IV);
    ABC_CHECK_NULL(pData);

    // create the final key
    unsigned char aKey[AES_256_KEY_LENGTH];
    memset(aKey, 0, AES_256_KEY_LENGTH);
    unsigned int keyLength = ABC_BUF_SIZE(Key);
    if (keyLength > AES_256_KEY_LENGTH)
    {
        keyLength = AES_256_KEY_LENGTH;
    }
    memcpy(aKey, ABC_BUF_PTR(Key), keyLength);

    // create the IV
    unsigned char aIV[AES_256_IV_LENGTH];
    memset(aIV, 0, AES_256_IV_LENGTH);
    unsigned int IVLength = ABC_BUF_SIZE(IV);
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
    int p_len = ABC_BUF_SIZE(EncData); 
    int f_len = 0;
    unsigned char *pTmpData = malloc(p_len + AES_256_BLOCK_LENGTH);
    
    // decrypt
    EVP_DecryptInit_ex(&d_ctx, NULL, NULL, NULL, NULL);
    EVP_DecryptUpdate(&d_ctx, pTmpData, &p_len, ABC_BUF_PTR(EncData), ABC_BUF_SIZE(EncData));
    EVP_DecryptFinal_ex(&d_ctx, pTmpData + p_len, &f_len);
    
    // set final values
    ABC_BUF_SET_PTR(*pData, pTmpData, p_len + f_len);
    
exit:

    return cc;
}

// creates a buffer of random data
tABC_CC ABC_CryptoCreateRandomData(unsigned int  length,
                                   tABC_U08Buf   *pData,
                                   tABC_Error    *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(pData);

    ABC_BUF_NEW(*pData, length);

    int rc = RAND_bytes(ABC_BUF_PTR(*pData), length);
    //unsigned long err = ERR_get_error();

    if (rc != 1) 
    {
        ABC_BUF_FREE(*pData);
        ABC_RET_ERROR(ABC_CC_Error, "Random data generation failed");
    }

exit:

    return cc;
}

tABC_CC ABC_CryptoHexEncode(const tABC_U08Buf Data,
                            char              **pszDataHex,
                            tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL(pszDataHex);

    char *szDataHex;
    unsigned int dataLength = ABC_BUF_SIZE(Data);
    szDataHex = calloc(1, (dataLength * 2) + 1);

    for (int i = 0; i < dataLength; i++)
    {
        unsigned char *pCurByte = (unsigned char *) (ABC_BUF_PTR(Data) + i);
        sprintf(&(szDataHex[i * 2]), "%02x", *pCurByte);
    }
    *pszDataHex = szDataHex;

exit:

    return cc;
}

tABC_CC ABC_CryptoHexDecode(const char  *szDataHex,
                            tABC_U08Buf *pData,
                            tABC_Error  *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szDataHex);
    ABC_CHECK_NULL(pData);

    unsigned int dataLength = strlen(szDataHex) / 2;

    ABC_BUF_NEW(*pData, dataLength);

    for (int i = 0; i < dataLength; i++)
    {
        unsigned int val;
        sscanf(&(szDataHex[i * 2]), "%02x", &val);
        unsigned char *pCurByte = (unsigned char *) (ABC_BUF_PTR(*pData) + i);
        *pCurByte = (unsigned char) val;
    }

exit:

    return cc;
}

tABC_CC ABC_CryptoBase64Encode(const tABC_U08Buf Data,
                               char              **pszDataBase64,
                               tABC_Error        *pError)
{ 
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL(pszDataBase64);

    BIO *bio, *b64;
    FILE *stream;
    unsigned int dataLength = ABC_BUF_SIZE(Data);
    int encodedSize = 4 * ceil((double)dataLength / 3);

    char *szDataBase64 = (char *)calloc(1, encodedSize + 1);
     
    stream = fmemopen(szDataBase64, encodedSize + 1, "w");
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_fp(stream, BIO_NOCLOSE);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Ignore newlines - write everything in one line
    BIO_write(bio, ABC_BUF_PTR(Data), ABC_BUF_SIZE(Data));
    BIO_flush(bio);
    BIO_free_all(bio);
    fclose(stream);

    *pszDataBase64 = szDataBase64;
     
exit:

    return cc;
}

tABC_CC ABC_CryptoBase64Decode(const char   *szDataBase64,
                               tABC_U08Buf  *pData,
                               tABC_Error   *pError)
{ 
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(szDataBase64);
    ABC_CHECK_NULL(pData);

    BIO *bio, *b64;
    int decodeLen = ABC_CryptoCalcBase64DecodeLength(szDataBase64);
    int len = 0;
    unsigned char *pTmpData = (unsigned char *) calloc(1, decodeLen + 1);

    FILE *stream = fmemopen((void *)szDataBase64, strlen(szDataBase64), "r");
     
    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new_fp(stream, BIO_NOCLOSE);
    bio = BIO_push(b64, bio);
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL); // Do not use newlines to flush buffer
    len = BIO_read(bio, pTmpData, strlen(szDataBase64));

    if (len != decodeLen)
    {
        ABC_RET_ERROR(ABC_CC_Error, "Base64 decode is incorrect");
    }
     
    BIO_free_all(bio);
    fclose(stream);

    ABC_BUF_SET_PTR(*pData, pTmpData, len);
     
exit:

    return cc;
}

// Calculates the length of a decoded base64 string
static 
int ABC_CryptoCalcBase64DecodeLength(const char *szDataBase64)
{
    int len = strlen(szDataBase64);
    int padding = 0;
     
    if (szDataBase64[len-1] == '=' && szDataBase64[len-2] == '=') //last two chars are =
    padding = 2;
    else if (szDataBase64[len-1] == '=') //last char is =
    padding = 1;
     
    return (int)len * 0.75 - padding;
}

// generates a randome UUID (version 4)
/*
Version 4 UUIDs use a scheme relying only on random numbers. 
This algorithm sets the version number (4 bits) as well as two reserved bits. 
All other bits (the remaining 122 bits) are set using a random or pseudorandom data source. 
Version 4 UUIDs have the form xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx where x is any hexadecimal 
digit and y is one of 8, 9, A, or B (e.g., f47ac10b-58cc-4372-a567-0e02b2c3d479).
*/
tABC_CC ABC_CryptoGenUUIDString(char       **pszUUID,
                                tABC_Error *pError)
{ 
    tABC_CC cc = ABC_CC_Ok;
    
    unsigned char *pData = NULL;

    ABC_CHECK_NULL(pszUUID);

    char *szUUID = calloc(1, UUID_STR_LENGTH + 1);

    tABC_U08Buf Data;
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(UUID_BYTE_COUNT, &Data, pError));
    pData = ABC_BUF_PTR(Data);

    // put in the version
    // To put in the version, take the 7th byte and perform an and operation using 0x0f, followed by an or operation with 0x40. 
    pData[6] = (pData[6] & 0xf) | 0x40;

    // 9th byte significant nibble is one of 8, 9, A, or B 
    pData[8] = (pData[8] | 0x80) & 0xbf;

    sprintf(szUUID, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            pData[0], pData[1], pData[2], pData[3], pData[4], pData[5], pData[6], pData[7], 
            pData[8], pData[9], pData[10], pData[11], pData[12], pData[13], pData[14], pData[15]);

    *pszUUID = szUUID;

exit:
    if (pData) free(pData);

    return cc;
}

// allocate and generate scrypt data with default vals and salt1
tABC_CC ABC_CryptoScryptS1(const tABC_U08Buf Data,
                           tABC_U08Buf       *pScryptData,
                           tABC_Error        *pError)
{ 
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL(pScryptData);

    tABC_U08Buf Salt;
    ABC_BUF_SET_PTR(Salt, gaS1, sizeof(gaS1));
    ABC_CHECK_RET(ABC_CryptoScrypt(Data,
                                   Salt,
                                   SCRYPT_DEFAULT_SERVER_N,
                                   SCRYPT_DEFAULT_SERVER_R,
                                   SCRYPT_DEFAULT_SERVER_P,
                                   SCRYPT_DEFAULT_LENGTH,
                                   pScryptData,
                                   pError));

exit:

    return cc;
}

// allocate and generate scrypt from an SNRP
tABC_CC ABC_CryptoScryptSNRP(const tABC_U08Buf     Data,
                             const tABC_CryptoSNRP *pSNRP,
                             tABC_U08Buf           *pScryptData,
                             tABC_Error            *pError)
{ 
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL(pSNRP);
    ABC_CHECK_NULL(pScryptData);

    ABC_CHECK_RET(ABC_CryptoScrypt(Data,
                                   pSNRP->Salt,
                                   pSNRP->N,
                                   pSNRP->r,
                                   pSNRP->p,
                                   SCRYPT_DEFAULT_LENGTH,
                                   pScryptData,
                                   pError));

exit:

    return cc;
}


// allocate and generate scrypt data given all vars
tABC_CC ABC_CryptoScrypt(const tABC_U08Buf Data,
                         const tABC_U08Buf Salt,
                         unsigned long     N,
                         unsigned long     r,
                         unsigned long     p,
                         unsigned int      scryptDataLength,
                         tABC_U08Buf       *pScryptData,
                         tABC_Error        *pError)
{ 
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Salt);
    ABC_CHECK_NULL(pScryptData);

    ABC_BUF_NEW(*pScryptData, scryptDataLength);

    int rc;
    if ((rc = crypto_scrypt(ABC_BUF_PTR(Data), ABC_BUF_SIZE(Data), ABC_BUF_PTR(Salt), ABC_BUF_SIZE(Salt), N, r, p, ABC_BUF_PTR(*pScryptData), scryptDataLength)) != 0)
    {
        ABC_BUF_FREE(*pScryptData);
        ABC_RET_ERROR(ABC_CC_ScryptError, "Error generating Scrypt data");
    }

exit:

    return cc;
}

// allocates an SNRP struct and fills it in with the info given to be used on the client side
// Note: the Salt buffer is copied 
tABC_CC ABC_CryptoCreateSNRPForClient(tABC_CryptoSNRP   **ppSNRP,
                                      tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf Salt = ABC_BUF_NULL;

    ABC_CHECK_NULL(ppSNRP);

    // gen some salt
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(SCRYPT_DEFAULT_SALT_LENGTH, &Salt, pError));

    ABC_CHECK_RET(ABC_CryptoCreateSNRP(Salt,
                                       SCRYPT_DEFAULT_CLIENT_N,
                                       SCRYPT_DEFAULT_CLIENT_R,
                                       SCRYPT_DEFAULT_CLIENT_P,
                                       ppSNRP,
                                       pError));
exit:
    ABC_BUF_FREE(Salt);

    return cc;
}

// allocates an SNRP struct and fills it in with the info given to be used on the server side
// Note: the Salt buffer is copied 
tABC_CC ABC_CryptoCreateSNRPForServer(tABC_CryptoSNRP   **ppSNRP,
                                      tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf Salt = ABC_BUF_NULL;

    ABC_CHECK_NULL(ppSNRP);

    // gen some salt
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(SCRYPT_DEFAULT_SALT_LENGTH, &Salt, pError));

    ABC_CHECK_RET(ABC_CryptoCreateSNRP(Salt,
                                       SCRYPT_DEFAULT_SERVER_N,
                                       SCRYPT_DEFAULT_SERVER_R,
                                       SCRYPT_DEFAULT_SERVER_P,
                                       ppSNRP,
                                       pError));
exit:
    ABC_BUF_FREE(Salt);

    return cc;
}


// allocates an SNRP struct and fills it in with the info given
// Note: the Salt buffer is copied 
tABC_CC ABC_CryptoCreateSNRP(const tABC_U08Buf Salt,
                             unsigned long     N,
                             unsigned long     r,
                             unsigned long     p,
                             tABC_CryptoSNRP   **ppSNRP,
                             tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_NULL_BUF(Salt);

    tABC_CryptoSNRP *pSNRP = malloc(sizeof(tABC_CryptoSNRP));

    // create a copy of the salt
    ABC_BUF_DUP(pSNRP->Salt, Salt);
    pSNRP->N = N;
    pSNRP->r = r;
    pSNRP->p = p;

    *ppSNRP = pSNRP;

exit:

    return cc;
}

// creates a jansson object for SNRP
tABC_CC ABC_CryptoCreateJSONObjectSNRP(const tABC_CryptoSNRP  *pSNRP,
                                       json_t                 **ppJSON_SNRP,
                                       tABC_Error             *pError)
{ 
    tABC_CC cc = ABC_CC_Ok;

    char    *szSalt_Hex     = NULL;

    ABC_CHECK_NULL(pSNRP);
    ABC_CHECK_NULL(ppJSON_SNRP);

    // encode the Salt into a Hex string
    ABC_CHECK_RET(ABC_CryptoHexEncode(pSNRP->Salt, &szSalt_Hex, pError));

    // create the jansson object
    *ppJSON_SNRP = json_pack("{sssisisi}", 
                             JSON_ENC_SALT_FIELD, szSalt_Hex,
                             JSON_ENC_N_FIELD, SCRYPT_DEFAULT_CLIENT_N,
                             JSON_ENC_R_FIELD, SCRYPT_DEFAULT_CLIENT_R,
                             JSON_ENC_P_FIELD, SCRYPT_DEFAULT_CLIENT_P);

exit:
    if (szSalt_Hex) free(szSalt_Hex);

    return cc;
}

// takes a jansson object representing an SNRP, decodes it and allocates a SNRP struct
tABC_CC ABC_CryptoDecodeJSONObjectSNRP(const json_t      *pJSON_SNRP,
                                       tABC_CryptoSNRP   **ppSNRP,
                                       tABC_Error        *pError)
{ 
    tABC_CC cc = ABC_CC_Ok;

    tABC_U08Buf Salt = ABC_BUF_NULL;

    ABC_CHECK_NULL(pJSON_SNRP);
    ABC_CHECK_NULL(ppSNRP);

    json_t *jsonVal;

    // get the salt
    jsonVal = json_object_get(pJSON_SNRP, JSON_ENC_SALT_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON SNRP - missing salt");
    const char *szSaltHex = json_string_value(jsonVal);

    // decrypt the salt
    ABC_CHECK_RET(ABC_CryptoHexDecode(szSaltHex, &Salt, pError));

    // get n
    jsonVal = json_object_get(pJSON_SNRP, JSON_ENC_N_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_number(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON SNRP - missing N");
    int N = (int) json_integer_value(jsonVal);

    // get r
    jsonVal = json_object_get(pJSON_SNRP, JSON_ENC_R_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_number(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON SNRP - missing r");
    int r = (int) json_integer_value(jsonVal);

    // get p
    jsonVal = json_object_get(pJSON_SNRP, JSON_ENC_P_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_number(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON SNRP - missing p");
    int p = (int) json_integer_value(jsonVal);

    // store final values
    tABC_CryptoSNRP *pSNRP = malloc(sizeof(tABC_CryptoSNRP));
    pSNRP->Salt = Salt;
    ABC_BUF_CLEAR(Salt); // so we don't free it when we leave
    pSNRP->N = N;
    pSNRP->r = r;
    pSNRP->p = p;
    *ppSNRP = pSNRP;

exit:
    ABC_BUF_FREE(Salt);

    return cc;
}

// deep free's an SNRP object including the Seed data
void ABC_CryptoFreeSNRP(tABC_CryptoSNRP **ppSNRP)
{
    tABC_CryptoSNRP *pSNRP = *ppSNRP;

    if (pSNRP)
    {
        ABC_BUF_FREE(pSNRP->Salt);
        free(pSNRP);
    }

    *ppSNRP = NULL;
}
