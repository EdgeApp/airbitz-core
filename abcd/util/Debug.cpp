/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Debug.hpp"
#include "FileIO.hpp"
#include "../Context.hpp"
#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#ifdef ANDROID
#include <android/log.h>
#endif
#include <iomanip>
#include <mutex>
#include <sstream>
#include <vector>

namespace abcd {

#define MAX_LOG_SIZE (1 << 20) // Max size 1 MiB

static std::mutex gDebugMutex;
static FILE *gLogFile = nullptr;

static std::string
debugLogPath()
{
    return gContext->rootDir() + "abc.log";
}

Status
debugInitialize()
{
#ifdef DEBUG
    std::lock_guard<std::mutex> lock(gDebugMutex);
    auto path = debugLogPath()
    gLogFile = fopen(path.c_str(), "w");
    if (!gLogFile)
        return ABC_ERROR(ABC_CC_SysError, "Cannot open " + path);
#endif

    return Status();
}

void
debugTerminate()
{
    std::lock_guard<std::mutex> lock(gDebugMutex);
    if (gLogFile)
        fclose(gLogFile);
}

DataChunk
debugLogLoad()
{
    DataChunk out;
    fileLoad(out, debugLogPath()).log();
    return out;
}

void ABC_DebugLog(const char *format, ...)
{
#ifdef DEBUG
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
    if (size < 0)
        return;

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

    std::lock_guard<std::mutex> lock(gDebugMutex);
    if (gLogFile)
    {
        if (MAX_LOG_SIZE < ftell(gLogFile))
        {
            fclose(gLogFile);
            gLogFile = fopen(debugLogPath().c_str(), "w");
        }

        fwrite(out.c_str(), 1, out.size(), gLogFile);
        fflush(gLogFile);
    }
#endif
}

} // namespace abcd
