/**
 * @file
 * AirBitz cryptographic function prototypes
 *
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
 *
 *  @author See AUTHORS
 *  @version 1.0
 */

#ifndef ABC_Crypto_h
#define ABC_Crypto_h

#include <jansson.h>
#include "ABC.h"
#include "ABC_Util.h"

namespace abcd {

#define AES_256_IV_LENGTH       16
#define AES_256_BLOCK_LENGTH    16
#define AES_256_KEY_LENGTH      32
#define SHA_256_LENGTH          32

#define HMAC_SHA_256_LENGTH     32
#define HMAC_SHA_512_LENGTH     64

    typedef enum eABC_CryptoType
    {
        ABC_CryptoType_AES256 = 0,
        ABC_CryptoType_AES256_Scrypt,
        ABC_CryptoType_Count
    } tABC_CryptoType;

    typedef struct sABC_CryptoSNRP
    {
        tABC_U08Buf     Salt;
        unsigned long   N;
        unsigned long   r;
        unsigned long   p;
    } tABC_CryptoSNRP;

    tABC_CC ABC_InitializeCrypto(tABC_Error        *pError);

    tABC_CC ABC_CryptoSetRandomSeed(const tABC_U08Buf Seed,
                                    tABC_Error        *pError);

    tABC_CC ABC_CryptoEncryptJSONString(const tABC_U08Buf Data,
                                        const tABC_U08Buf Key,
                                        tABC_CryptoType   cryptoType,
                                        char              **pszEncDataJSON,
                                        tABC_Error        *pError);

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

    tABC_CC ABC_CryptoDecryptJSONString(const char        *szEncDataJSON,
                                        const tABC_U08Buf Key,
                                        tABC_U08Buf       *pData,
                                        tABC_Error        *pError);

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

    tABC_CC ABC_CryptoCreateRandomData(unsigned int  length,
                                       tABC_U08Buf   *pData,
                                       tABC_Error    *pError);

    tABC_CC ABC_CryptoHexEncode(const tABC_U08Buf Data,
                                char              **pszDataHex,
                                tABC_Error        *pError);

    tABC_CC ABC_CryptoHexDecode(const char  *szDataHex,
                                tABC_U08Buf *pData,
                                tABC_Error  *pError);

    tABC_CC ABC_CryptoBase64Encode(const tABC_U08Buf Data,
                                   char              **pszDataBase64,
                                   tABC_Error        *pError);

    tABC_CC ABC_CryptoBase64Decode(const char   *szDataBase64,
                                   tABC_U08Buf  *pData,
                                   tABC_Error   *pError);

    tABC_CC ABC_CryptoGenUUIDString(char       **pszUUID,
                                    tABC_Error *pError);

    tABC_CC ABC_CryptoScryptS1(const tABC_U08Buf Data,
                               tABC_U08Buf       *pScryptData,
                               tABC_Error        *pError);

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

    void ABC_CryptoFreeSNRP(tABC_CryptoSNRP **ppSNRP);

    tABC_CC ABC_CryptoHMAC256(tABC_U08Buf Data,
                              tABC_U08Buf Key,
                              tABC_U08Buf *pDataHMAC,
                              tABC_Error  *pError);

    tABC_CC ABC_CryptoHMAC512(tABC_U08Buf Data,
                              tABC_U08Buf Key,
                              tABC_U08Buf *pDataHMAC,
                              tABC_Error  *pError);

} // namespace abcd

#endif
