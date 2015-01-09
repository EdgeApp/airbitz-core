/**
 * @fileDebug * AirBitz utility function prototypes
 *
 * This file contains misc debug functions
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
#include "ABC_Debug.h"

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdarg.h>
#include <pthread.h>
#include "ABC_Util.h"
#ifdef ANDROID
#include <android/log.h>
#endif

namespace abcd {

#ifdef DEBUG

#define BUF_SIZE 16384
#define MAX_LOG_SIZE 102400 // Max size 100 KB

#define ABC_LOG_FILE "abc.log"

static void ABC_DebugAppendToLog(const char *szOut);
static tABC_CC ABC_DebugMutexLock(tABC_Error *pError);
static tABC_CC ABC_DebugMutexUnlock(tABC_Error *pError);

static FILE             *gfLog = NULL;
static bool             gbInitialized = false;
static pthread_mutex_t  gMutex;
char gszLogFile[ABC_MAX_STRING_LENGTH + 1];

tABC_CC ABC_DebugInitialize(const char *szRootDir, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    ABC_CHECK_NULL(szRootDir);

    snprintf(gszLogFile, ABC_MAX_STRING_LENGTH, "%s/%s", szRootDir, ABC_LOG_FILE);
    gszLogFile[ABC_MAX_STRING_LENGTH] = '\0';

    pthread_mutexattr_t mutexAttrib;
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_init(&mutexAttrib), ABC_CC_MutexError, "ABC_Debug could not create mutex attribute");
    ABC_CHECK_ASSERT(0 == pthread_mutexattr_settype(&mutexAttrib, PTHREAD_MUTEX_RECURSIVE), ABC_CC_MutexError, "ABC_Debug could not set mutex attributes");
    ABC_CHECK_ASSERT(0 == pthread_mutex_init(&gMutex, &mutexAttrib), ABC_CC_MutexError, "ABC_Debug could not create mutex");
    pthread_mutexattr_destroy(&mutexAttrib);

    gbInitialized = true;

    gfLog = fopen(gszLogFile, "a");
    ABC_CHECK_SYS(gfLog, "fopen(log file)");
    fseek(gfLog, 0L, SEEK_END);
exit:
    return cc;
}

void ABC_DebugTerminate()
{
    if (gbInitialized == true)
    {
        pthread_mutex_destroy(&gMutex);
        if (gfLog)
        {
            fclose(gfLog);
        }
        gbInitialized = false;
    }
}

tABC_CC ABC_DebugLogFilename(char **szFilename, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

	ABC_CHECK_NULL(szFilename);
	ABC_STRDUP(*szFilename, gszLogFile);

exit:
    return cc;
}

void ABC_DebugLog(const char * format, ...)
{
    static char szOut[BUF_SIZE];
    struct tm *	newtime;
    time_t		t;

    time(&t);                /* Get time as long integer. */
    newtime = localtime(&t); /* Convert to local time. */

    snprintf(szOut, BUF_SIZE, "%d-%02d-%02d %.2d:%.2d:%.2d ABC_Log: ",
            newtime->tm_year + 1900,
            newtime->tm_mon + 1,
            newtime->tm_mday,
            newtime->tm_hour, newtime->tm_min, newtime->tm_sec);

    va_list	args;
    va_start(args, format);
    vsnprintf(&(szOut[strlen(szOut)]), BUF_SIZE, format, args);
    // if it doesn't end in an newline, add it
    if (szOut[strlen(szOut) - 1] != '\n')
    {
        szOut[strlen(szOut) + 1] = '\0';
        szOut[strlen(szOut)] = '\n';
    }
#ifdef ANDROID
    __android_log_print(ANDROID_LOG_DEBUG, "ABC", "%s", szOut);
#else
    printf("%s", szOut);
#endif
    va_end(args);
    ABC_DebugAppendToLog(szOut);
}

static void ABC_DebugAppendToLog(const char *szOut)
{
    if (gbInitialized)
    {
        ABC_DebugMutexLock(NULL);
        if (gfLog)
        {
            size_t size = ftell(gfLog);
            if (size > MAX_LOG_SIZE)
            {
                fclose(gfLog);
                gfLog = fopen(gszLogFile, "w");
            }

            fwrite(szOut, 1, strlen(szOut), gfLog);
            fflush(gfLog);
        }
        ABC_DebugMutexUnlock(NULL);
    }
}

static
tABC_CC ABC_DebugMutexLock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_Debug has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_lock(&gMutex), ABC_CC_MutexError, "ABC_Debug error locking mutex");

exit:

    return cc;
}

static
tABC_CC ABC_DebugMutexUnlock(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_CHECK_ASSERT(true == gbInitialized, ABC_CC_NotInitialized, "ABC_Debug has not been initalized");
    ABC_CHECK_ASSERT(0 == pthread_mutex_unlock(&gMutex), ABC_CC_MutexError, "ABC_Debug error unlocking mutex");

exit:

    return cc;
}

#else

tABC_CC ABC_DebugInitialize(const char *szRootDir, tABC_Error *pError)
{
}

void ABC_DebugTerminate()
{
}

/**
 * Log string placeholder for non-debug build
 */
void ABC_DebugLog(const char * format, ...)
{
}

#endif

} // namespace abcd
