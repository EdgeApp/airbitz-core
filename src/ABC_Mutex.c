/**
 * @file
 * AirBitz mutex function prototypes
 *
 * This file contains mutex functions to allow mutiple modules to
 * share a single recursive mutex.
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <pthread.h>
#include "ABC.h"
#include "ABC_Util.h"

static bool             gbInitialized = false;
static pthread_mutex_t  gMutex; // to block multiple threads from accessing resources at the same time

/**
 * Initialize the Mutex system
 */
tABC_CC ABC_MutexInitialize(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_ASSERT(false == gbInitialized, ABC_CC_Reinitialization, "ABC_Mutex has already been initalized");

    // create a mutex to block multiple threads from accessing files at the same time
    pthread_mutexattr_t mutexAttrib;
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_init(&mutexAttrib), ABC_CC_MutexError, "ABC_Mutex could not create mutex attribute");
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_settype(&mutexAttrib, PTHREAD_MUTEX_RECURSIVE), ABC_CC_MutexError, "ABC_FileIO could not set mutex attributes");
    ABC_CHECK_ASSERT(0 == pthread_mutex_init(&gMutex, &mutexAttrib), ABC_CC_MutexError, "ABC_Mutex could not create mutex");
    pthread_mutexattr_destroy(&mutexAttrib);

    gbInitialized = true;

exit:

    return cc;
}

/**
 * Shut down the Mutex system
 */
void ABC_MutexTerminate()
{
    if (gbInitialized == true)
    {
        pthread_mutex_destroy(&gMutex);

        gbInitialized = false;
    }
}

/**
 * Locks the global mutex
 *
 */
tABC_CC ABC_MutexLock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_Mutex has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_lock(&gMutex), ABC_CC_MutexError, "ABC_Mutex error locking mutex");

exit:

    return cc;
}

/**
 * Unlocks the global mutex
 *
 */
tABC_CC ABC_MutexUnlock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_Mutex has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_unlock(&gMutex), ABC_CC_MutexError, "ABC_Mutex error unlocking mutex");

exit:
    
    return cc;
}

