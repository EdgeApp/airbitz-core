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
#include "ABC_Util.h"

#define ABC_BRIDGE_INVALID_AMOUNT           ((int64_t)-1)
#define ABC_BRIDGE_BITCOIN_DECIMAL_PLACE    8

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_BridgeParseBitcoinURI(const char *szURI,
                                tABC_BitcoinURIInfo **ppInfo,
                                tABC_Error *pError);

    void ABC_BridgeFreeURIInfo(tABC_BitcoinURIInfo *pInfo);

    tABC_CC ABC_BridgeParseAmount(const char *szAmount,
                                  int64_t *pValue,
                                  unsigned decimal_places);

    tABC_CC ABC_BridgeFormatAmount(int64_t amount,
                                   char **pszAmountOut,
                                   unsigned decimal_places);

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

#ifdef __cplusplus
}
#endif

#endif
