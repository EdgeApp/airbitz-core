/**
 * @file
 * AirBitz C++ bridge function prototypes
 *
 * @author William Swanson
 * @version 0.1
 */

#ifndef ABC_Bridge_h
#define ABC_Bridge_h

#include "ABC.h"
#include "ABC_Account.h"
#include "ABC_Tx.h"
#include "ABC_Util.h"

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_BridgeParseBitcoinURI(const char *szURI,
                                tABC_BitcoinURIInfo **ppInfo,
                                tABC_Error *pError);

    void ABC_BridgeFreeURIInfo(tABC_BitcoinURIInfo *pInfo);

    tABC_CC ABC_BridgeParseAmount(const char *szAmount,
                                  int64_t *pAmountOut,
                                  unsigned decimalPlaces);

    tABC_CC ABC_BridgeFormatAmount(int64_t amount,
                                   char **pszAmountOut,
                                   unsigned decimalPlaces,
                                   tABC_Error *pError);

    tABC_CC ABC_BridgeEncodeBitcoinURI(char **pszURI,
                                       tABC_BitcoinURIInfo *pInfo,
                                       tABC_Error *pError);

    tABC_CC ABC_BridgeBase58Encode(tABC_U08Buf Data,
                                   char **pszBase58,
                                   tABC_Error *pError);

    tABC_CC ABC_BridgeBase58Decode(const char *szBase58,
                                   tABC_U08Buf *pData,
                                   tABC_Error *pError);

    tABC_CC ABC_BridgeGetBitcoinPubAddress(char **pszPubAddress,
                                           tABC_U08Buf PrivateSeed,
                                           int32_t N,
                                           tABC_Error *pError);

    tABC_CC ABC_BridgeGetBitcoinPrivAddress(char **pszPrivAddress,
                                            tABC_U08Buf PrivateSeed,
                                            int32_t N,
                                            tABC_Error *pError);

    tABC_CC ABC_BridgeWatcherStart(const char *szUserName,
                                   const char *szPassword,
                                   const char *walletUUID,
                                   tABC_Error *pError);

    tABC_CC ABC_BridgeWatcherStop(const char *szWalletUUID, tABC_Error *pError);

    tABC_CC ABC_BridgeWatcherRestart(const char *szUserName,
                                     const char *szPassword,
                                     const char *szWalletUUID,
                                     bool clearCache, tABC_Error *pError);

    tABC_CC ABC_BridgeWatchAddr(const char *szUserName, const char *szPassword,
                                const char *walletUUID, const char *address,
                                bool prioritize, tABC_Error *pError);

    tABC_CC ABC_BridgeTxMake(tABC_TxSendInfo *pSendInfo,
                             char **addresses, int addressCount,
                             char *changeAddress,
                             tABC_UnsignedTx *pUtx,
                             tABC_Error *pError);

    tABC_CC ABC_BridgeTxSignSend(tABC_TxSendInfo *pSendInfo,
                                 char **paPrivKey,
                                 unsigned int keyCount,
                                 tABC_UnsignedTx *pUtx,
                                 tABC_Error *pError);

    tABC_CC ABC_BridgeMaxSpendable(const char *szUsername,
                                   const char *szPassword,
                                   const char *szWalletUUID,
                                   const char *szDestAddress,
                                   bool bTransfer,
                                   uint64_t *pMaxSatoshi,
                                   tABC_Error *pError);

    tABC_CC ABC_BridgeTxHeight(const char *szWalletUUID, const char *szTxId, unsigned int *height, tABC_Error *pError);

    tABC_CC ABC_BridgeTxBlockHeight(const char *szWalletUUID, unsigned int *height, tABC_Error *pError);

    bool ABC_BridgeIsTestNet();

#ifdef __cplusplus
}
#endif

#endif
