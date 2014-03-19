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

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_BridgeParseBitcoinURI(const char *szURI,
                                tABC_BitcoinURIInfo **ppInfo,
                                tABC_Error *pError);

    void ABC_BridgeFreeURIInfo(tABC_BitcoinURIInfo *pInfo);

    tABC_CC ABC_BridgeEncodeBitcoinURI(char **pszURI,
                                       tABC_BitcoinURIInfo *pInfo,
                                       tABC_Error *pError);

    tABC_CC ABC_BridgeBase58Encode(tABC_U08Buf Data, char **pszBase58, tABC_Error *pError);
    tABC_CC ABC_BridgeBase58Decode(const char *szBase58, tABC_U08Buf *pData, tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
