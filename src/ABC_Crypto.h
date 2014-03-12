/**
 * @file
 * AirBitz cryptographic function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_Crypto_h
#define ABC_Crypto_h

#include <jansson.h>
#include "ABC.h"
#include "ABC_Util.h"

#define AES_256_IV_LENGTH       16
#define AES_256_BLOCK_LENGTH    16
#define AES_256_KEY_LENGTH      32
#define SHA_256_LENGTH          32

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif
