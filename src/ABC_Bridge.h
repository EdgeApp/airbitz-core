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

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_BridgeParseBitcoinURI(const char *szURI,
                                tABC_BitcoinURIInfo **ppInfo,
                                tABC_Error *pError);

    void ABC_BridgeFreeURIInfo(tABC_BitcoinURIInfo *pInfo);

#ifdef __cplusplus
}
#endif

#endif
