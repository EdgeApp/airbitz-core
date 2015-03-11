/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABCD_CRYPTO_SCRYPT_HPP
#define ABCD_CRYPTO_SCRYPT_HPP

#include "../util/U08Buf.hpp"
#include "../../src/ABC.h"
#include <jansson.h>

namespace abcd {

typedef struct sABC_CryptoSNRP
{
    tABC_U08Buf     Salt;
    unsigned long   N;
    unsigned long   r;
    unsigned long   p;
} tABC_CryptoSNRP;

tABC_CC ABC_InitializeCrypto(tABC_Error        *pError);

tABC_CC ABC_CryptoScryptSNRP(const tABC_U08Buf     Data,
                             const tABC_CryptoSNRP *pSNRP,
                             tABC_U08Buf           *pScryptData,
                             tABC_Error            *pError);

tABC_CC ABC_CryptoScrypt(const tABC_U08Buf Data,
                         const tABC_U08Buf Salt,
                         unsigned long     N,
                         unsigned long     r,
                         unsigned long     p,
                         unsigned int      scryptDataLength,
                         tABC_U08Buf       *pScryptData,
                         tABC_Error        *pError);

tABC_CC ABC_CryptoCreateSNRPForClient(tABC_CryptoSNRP   **ppSNRP,
                                      tABC_Error        *pError);

tABC_CC ABC_CryptoCreateSNRPForServer(tABC_CryptoSNRP   **ppSNRP,
                                      tABC_Error        *pError);

tABC_CC ABC_CryptoCreateSNRP(const tABC_U08Buf Salt,
                             unsigned long     N,
                             unsigned long     r,
                             unsigned long     p,
                             tABC_CryptoSNRP   **ppSNRP,
                             tABC_Error        *pError);

tABC_CC ABC_CryptoCreateJSONObjectSNRP(const tABC_CryptoSNRP  *pSNRP,
                                       json_t                 **ppJSON_SNRP,
                                       tABC_Error             *pError);

tABC_CC ABC_CryptoDecodeJSONObjectSNRP(const json_t      *pJSON_SNRP,
                                       tABC_CryptoSNRP   **ppSNRP,
                                       tABC_Error        *pError);

void ABC_CryptoFreeSNRP(tABC_CryptoSNRP *pSNRP);

} // namespace abcd

#endif
