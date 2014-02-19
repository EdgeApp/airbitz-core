/**
 * @file
 * AirBitz URL function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_URL_h
#define ABC_URL_h

#include "ABC.h"
#include "ABC_Util.h"


#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_URLInitialize(tABC_Error *pError);

    void ABC_URLTerminate();

    tABC_CC ABC_URLRequest(const char *szURL, const char *szPostData, tABC_U08Buf *pData, tABC_Error *pError);


#ifdef __cplusplus
}
#endif

#endif
