/**
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
 */

#include "Debug.hpp"
#include "FileIO.hpp"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#ifdef ANDROID
#include <android/log.h>
#endif
#include <iomanip>
#include <mutex>
#include <sstream>

namespace abcd {

#ifdef DEBUG

#define MAX_LOG_SIZE 102400 // Max size 100 KiB
#define ABC_LOG_FILE "abc.log"

static std::recursive_mutex gDebugMutex;
static std::string gLogFilename;
static FILE *gLogFile = nullptr;

static void ABC_DebugAppendToLog(const char *szOut);

tABC_CC ABC_DebugInitialize(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    gLogFilename = getRootDir() + ABC_LOG_FILE;

    gLogFile = fopen(gLogFilename.c_str(), "a");
    ABC_CHECK_SYS(gLogFile, "fopen(log file)");
    fseek(gLogFile, 0L, SEEK_END);

exit:
    return cc;
}

void ABC_DebugTerminate()
{
    if (gLogFile)
        fclose(gLogFile);
}

tABC_CC ABC_DebugLogFilename(char **szFilename, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    ABC_STRDUP(*szFilename, gLogFilename.c_str());

exit:
    return cc;
}

void ABC_DebugLog(const char *format, ...)
{
    time_t t = time(nullptr);
    struct tm *utc = gmtime(&t);

    std::stringstream date;
    date << std::setfill('0');
    date << std::setw(4) << utc->tm_year + 1900 << '-';
    date << std::setw(2) << utc->tm_mon + 1 << '-';
    date << std::setw(2) << utc->tm_mday << ' ';
    date << std::setw(2) << utc->tm_hour << ':';
    date << std::setw(2) << utc->tm_min << ':';
    date << std::setw(2) << utc->tm_sec << " ABC_Log: ";

    // Get the message length:
    va_list args;
    va_start(args, format);
    char temp[1];
    int size = vsnprintf(temp, sizeof(temp), format, args);
    va_end(args);

    // Format the message:
    va_start(args, format);
    std::vector<char> message(size + 1);
    vsnprintf(message.data(), message.size(), format, args);
    va_end(args);

    // Put the pieces together:
    std::string out = date.str();
    out.append(message.begin(), message.end() - 1);
    if (out.back() != '\n')
        out.append(1, '\n');

#ifdef ANDROID
    __android_log_print(ANDROID_LOG_DEBUG, "ABC", "%s", out.c_str());
#else
    printf("%s", out.c_str());
#endif
    ABC_DebugAppendToLog(out.c_str());
}

static void ABC_DebugAppendToLog(const char *szOut)
{
    if (gLogFile)
    {
        std::lock_guard<std::recursive_mutex> lock(gDebugMutex);
        size_t size = ftell(gLogFile);
        if (size > MAX_LOG_SIZE)
        {
            fclose(gLogFile);
            gLogFile = fopen(gLogFilename.c_str(), "w");
        }

        fwrite(szOut, 1, strlen(szOut), gLogFile);
        fflush(gLogFile);
    }
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
