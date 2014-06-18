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

#define ABC_URL_MAX_PATH_LENGTH 2048

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_URLInitialize(tABC_Error *pError);

    void ABC_URLTerminate();

    tABC_CC ABC_URLRequest(const char *szURL,
                           tABC_U08Buf *pData,
                           tABC_Error *pError);

    tABC_CC ABC_URLRequestString(const char *szURL,
                                 char **pszResults,
                                 tABC_Error *pError);

    tABC_CC ABC_URLPost(const char *szURL,
                        const char *szPostData,
                        tABC_U08Buf *pData,
                        tABC_Error *pError);

    tABC_CC ABC_URLPostString(const char *szURL,
                              const char *szPostData,
                              char **pszResults,
                              tABC_Error *pError);

#ifdef __cplusplus
}
#endif

#endif
