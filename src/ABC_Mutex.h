/**
 * @file
 * AirBitz Mutex function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_Mutex_h
#define ABC_Mutex_h

#include "ABC.h"

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_MutexInitialize(tABC_Error *pError);

    void ABC_MutexTerminate();

    tABC_CC ABC_MutexLock(tABC_Error *pError);

    tABC_CC ABC_MutexUnlock(tABC_Error *pError);


#ifdef __cplusplus
}
#endif

#endif
