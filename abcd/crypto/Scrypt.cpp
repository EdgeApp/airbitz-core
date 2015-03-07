/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Scrypt.hpp"
#include "Crypto.hpp"
#include "Random.hpp"
#include "../bitcoin/Testnet.hpp"
#include "../../minilibs/scrypt/crypto_scrypt.h"
#include <sys/time.h>

namespace abcd {

#define JSON_ENC_SALT_FIELD     "salt_hex"
#define JSON_ENC_N_FIELD        "n"
#define JSON_ENC_R_FIELD        "r"
#define JSON_ENC_P_FIELD        "p"

#define SCRYPT_DEFAULT_SERVER_N    16384    // can't change as server uses this as well
#define SCRYPT_DEFAULT_SERVER_R    1        // can't change as server uses this as well
#define SCRYPT_DEFAULT_SERVER_P    1        // can't change as server uses this as well
#define SCRYPT_DEFAULT_CLIENT_N    16384
#define SCRYPT_DEFAULT_CLIENT_R    1
#define SCRYPT_DEFAULT_CLIENT_P    1
#define SCRYPT_MAX_CLIENT_N        (1 << 17)
#define SCRYPT_TARGET_USECONDS     500000

#define SCRYPT_DEFAULT_LENGTH      32
#define SCRYPT_DEFAULT_SALT_LENGTH 32

#define TIMED_SCRYPT_PARAMS        TRUE

//
// Eeewww. Global var. Should we mutex this? It's just a single initialized var at
// startup. It's never written after that. Only read.
//
unsigned int g_timedScryptN = SCRYPT_DEFAULT_CLIENT_N;
unsigned int g_timedScryptR = SCRYPT_DEFAULT_CLIENT_R;

static unsigned char gaS1[] = { 0xb5, 0x86, 0x5f, 0xfb, 0x9f, 0xa7, 0xb3, 0xbf, 0xe4, 0xb2, 0x38, 0x4d, 0x47, 0xce, 0x83, 0x1e, 0xe2, 0x2a, 0x4a, 0x9d, 0x5c, 0x34, 0xc7, 0xef, 0x7d, 0x21, 0x46, 0x7c, 0xc7, 0x58, 0xf8, 0x1b };

// Testnet salt. Just has to be different from mainnet salt so we can create users
// with same login that exist on both testnet and mainnet and don't conflict
static unsigned char gaS1_testnet[] = { 0xa5, 0x96, 0x3f, 0x3b, 0x9c, 0xa6, 0xb3, 0xbf, 0xe4, 0xb2, 0x36, 0x42, 0x37, 0xfe, 0x87, 0x1e, 0xf2, 0x2a, 0x4a, 0x9d, 0x4c, 0x34, 0xa7, 0xef, 0x3d, 0x21, 0x47, 0x8c, 0xc7, 0x58, 0xf8, 0x1b };

/*
 * Initializes Scrypt paramenters by benchmarking device
 */
tABC_CC ABC_InitializeCrypto(tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    struct timeval timerStart;
    struct timeval timerEnd;
    int totalTime;
    tABC_U08Buf Salt; // Do not free
    AutoU08Buf temp;

    ABC_DebugLog("%s called", __FUNCTION__);

    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    if (isTestnet())
    {
        ABC_BUF_SET_PTR(Salt, gaS1_testnet, sizeof(gaS1));
    }
    else
    {
        ABC_BUF_SET_PTR(Salt, gaS1, sizeof(gaS1));
    }
    gettimeofday(&timerStart, NULL);
    ABC_CHECK_RET(ABC_CryptoScrypt(Salt,
                                   Salt,
                                   SCRYPT_DEFAULT_CLIENT_N,
                                   SCRYPT_DEFAULT_CLIENT_R,
                                   SCRYPT_DEFAULT_CLIENT_P,
                                   SCRYPT_DEFAULT_LENGTH,
                                   &temp,
                                   pError));
    gettimeofday(&timerEnd, NULL);

    // Totaltime is in uSec
    totalTime = 1000000 * (timerEnd.tv_sec - timerStart.tv_sec);
    totalTime += timerEnd.tv_usec;
    totalTime -= timerStart.tv_usec;

#ifdef TIMED_SCRYPT_PARAMS
    if (totalTime >= SCRYPT_TARGET_USECONDS)
    {
        // Very slow device.
        // Do nothing, use default scrypt settings which are lowest we'll go
    }
    else if (totalTime >= SCRYPT_TARGET_USECONDS / 8)
    {
        // Medium speed device.
        // Scale R between 1 to 8 assuming linear effect on hashing time.
        // Don't touch N.
        g_timedScryptR = SCRYPT_TARGET_USECONDS / totalTime;
    }
    else if (totalTime > 0)
    {
        // Very fast device.
        g_timedScryptR = 8;

        // Need to adjust scryptN to make scrypt even stronger:
        unsigned int temp = (SCRYPT_TARGET_USECONDS / 8) / totalTime;
        g_timedScryptN <<= (temp - 1);
        if (SCRYPT_MAX_CLIENT_N < g_timedScryptN || !g_timedScryptN)
        {
            g_timedScryptN = SCRYPT_MAX_CLIENT_N;
        }
    }
#endif

    ABC_DebugLog("Scrypt timing: %d\n", totalTime);
    ABC_DebugLog("Scrypt N = %d\n",g_timedScryptN);
    ABC_DebugLog("Scrypt R = %d\n",g_timedScryptR);

exit:

    return cc;
}

/**
 * Allocate and generate scrypt from an SNRP
 */
tABC_CC ABC_CryptoScryptSNRP(const tABC_U08Buf     Data,
                             const tABC_CryptoSNRP *pSNRP,
                             tABC_U08Buf           *pScryptData,
                             tABC_Error            *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

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

/**
 * Allocate and generate scrypt data given all vars
 */
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
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL_BUF(Salt);
    ABC_CHECK_NULL(pScryptData);

    ABC_BUF_NEW(*pScryptData, scryptDataLength);

    int rc;
    if ((rc = crypto_scrypt(ABC_BUF_PTR(Data), ABC_BUF_SIZE(Data), ABC_BUF_PTR(Salt), ABC_BUF_SIZE(Salt), N, (uint32_t) r, (uint32_t) p, ABC_BUF_PTR(*pScryptData), scryptDataLength)) != 0)
    {
        ABC_BUF_FREE(*pScryptData);
        ABC_RET_ERROR(ABC_CC_ScryptError, "Error generating Scrypt data");
    }

exit:

    return cc;
}

/**
 * Allocates an SNRP struct and fills it in with the info given to be used on the client side
 * Note: the Salt buffer is copied
 */
tABC_CC ABC_CryptoCreateSNRPForClient(tABC_CryptoSNRP   **ppSNRP,
                                      tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoU08Buf Salt;

    ABC_CHECK_NULL(ppSNRP);

    // gen some salt
    ABC_CHECK_RET(ABC_CryptoCreateRandomData(SCRYPT_DEFAULT_SALT_LENGTH, &Salt, pError));

    ABC_CHECK_RET(ABC_CryptoCreateSNRP(Salt,
                                       g_timedScryptN,
                                       g_timedScryptR,
                                       SCRYPT_DEFAULT_CLIENT_P,
                                       ppSNRP,
                                       pError));
exit:
    return cc;
}

/**
 * Allocates an SNRP struct and fills it in with the info given to be used on the server side
 * Note: the Salt buffer is copied from the global server salt
 */
tABC_CC ABC_CryptoCreateSNRPForServer(tABC_CryptoSNRP   **ppSNRP,
                                      tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_U08Buf Salt = ABC_BUF_NULL; // Do not free

    ABC_CHECK_NULL(ppSNRP);

    // get the server salt
    if (isTestnet())
    {
        ABC_BUF_SET_PTR(Salt, gaS1_testnet, sizeof(gaS1));
    }
    else
    {
        ABC_BUF_SET_PTR(Salt, gaS1, sizeof(gaS1));
    }

    ABC_CHECK_RET(ABC_CryptoCreateSNRP(Salt,
                                       SCRYPT_DEFAULT_SERVER_N,
                                       SCRYPT_DEFAULT_SERVER_R,
                                       SCRYPT_DEFAULT_SERVER_P,
                                       ppSNRP,
                                       pError));
exit:

    return cc;
}

/**
 * Allocates an SNRP struct and fills it in with the info given
 * Note: the Salt buffer is copied
 */
tABC_CC ABC_CryptoCreateSNRP(const tABC_U08Buf Salt,
                             unsigned long     N,
                             unsigned long     r,
                             unsigned long     p,
                             tABC_CryptoSNRP   **ppSNRP,
                             tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_CryptoSNRP *pSNRP = NULL;

    ABC_CHECK_NULL_BUF(Salt);

    ABC_NEW(pSNRP, tABC_CryptoSNRP);

    // create a copy of the salt
    ABC_BUF_DUP(pSNRP->Salt, Salt);
    pSNRP->N = N;
    pSNRP->r = r;
    pSNRP->p = p;

    *ppSNRP = pSNRP;

exit:

    return cc;
}

/**
 * Creates a jansson object for SNRP
 */
tABC_CC ABC_CryptoCreateJSONObjectSNRP(const tABC_CryptoSNRP  *pSNRP,
                                       json_t                 **ppJSON_SNRP,
                                       tABC_Error             *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    char    *szSalt_Hex     = NULL;

    ABC_CHECK_NULL(pSNRP);
    ABC_CHECK_NULL(ppJSON_SNRP);

    // encode the Salt into a Hex string
    ABC_CHECK_RET(ABC_CryptoHexEncode(pSNRP->Salt, &szSalt_Hex, pError));

    // create the jansson object
    *ppJSON_SNRP = json_pack("{sssisisi}",
                             JSON_ENC_SALT_FIELD, szSalt_Hex,
                             JSON_ENC_N_FIELD, pSNRP->N,
                             JSON_ENC_R_FIELD, pSNRP->r,
                             JSON_ENC_P_FIELD, pSNRP->p);

exit:
    ABC_FREE_STR(szSalt_Hex);

    return cc;
}

/**
 * Takes a jansson object representing an SNRP, decodes it and allocates a SNRP struct
 */
tABC_CC ABC_CryptoDecodeJSONObjectSNRP(const json_t      *pJSON_SNRP,
                                       tABC_CryptoSNRP   **ppSNRP,
                                       tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoU08Buf Salt;
    tABC_CryptoSNRP *pSNRP = NULL;
    const char *szSaltHex = NULL;
    unsigned long N, r, p;

    ABC_CHECK_NULL(pJSON_SNRP);
    ABC_CHECK_NULL(ppSNRP);

    json_t *jsonVal;

    // get the salt
    jsonVal = json_object_get(pJSON_SNRP, JSON_ENC_SALT_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_string(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON SNRP - missing salt");
    szSaltHex = json_string_value(jsonVal);

    // decrypt the salt
    ABC_CHECK_RET(ABC_CryptoHexDecode(szSaltHex, &Salt, pError));

    // get n
    jsonVal = json_object_get(pJSON_SNRP, JSON_ENC_N_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_number(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON SNRP - missing N");
    N = (unsigned long) json_integer_value(jsonVal);

    // get r
    jsonVal = json_object_get(pJSON_SNRP, JSON_ENC_R_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_number(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON SNRP - missing r");
    r = (unsigned long) json_integer_value(jsonVal);

    // get p
    jsonVal = json_object_get(pJSON_SNRP, JSON_ENC_P_FIELD);
    ABC_CHECK_ASSERT((jsonVal && json_is_number(jsonVal)), ABC_CC_DecryptError, "Error parsing JSON SNRP - missing p");
    p = (unsigned long) json_integer_value(jsonVal);

    // store final values
    ABC_NEW(pSNRP, tABC_CryptoSNRP);
    pSNRP->Salt = Salt;
    ABC_BUF_CLEAR(Salt); // so we don't free it when we leave
    pSNRP->N = N;
    pSNRP->r = r;
    pSNRP->p = p;
    *ppSNRP = pSNRP;

exit:
    return cc;
}

/**
 * Deep free's an SNRP object including the Seed data
 */
void ABC_CryptoFreeSNRP(tABC_CryptoSNRP *pSNRP)
{
    if (pSNRP)
    {
        ABC_BUF_FREE(pSNRP->Salt);
        ABC_CLEAR_FREE(pSNRP, sizeof(tABC_CryptoSNRP));
    }
}

} // namespace abcd
