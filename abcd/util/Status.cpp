/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */
#include "Status.hpp"

#include <string.h>

namespace abcd {

Status::Status() :
    value_(ABC_CC_Ok),
    line_(0)
{
}

Status::Status(tABC_CC value, std::string message,
    const char *file, const char *function, size_t line) :
    value_(value),
    message_(message),
    file_(file),
    function_(function),
    line_(line)
{
}

void Status::toError(tABC_Error &error)
{
    error.code = value_;
    strncpy(error.szDescription, message_.c_str(), ABC_MAX_STRING_LENGTH);
    strncpy(error.szSourceFunc, function_, ABC_MAX_STRING_LENGTH);
    strncpy(error.szSourceFile, file_, ABC_MAX_STRING_LENGTH);
    error.nSourceLine = line_;

    error.szDescription[ABC_MAX_STRING_LENGTH] = 0;
    error.szSourceFunc[ABC_MAX_STRING_LENGTH] = 0;
    error.szSourceFile[ABC_MAX_STRING_LENGTH] = 0;
}

Status Status::fromError(const tABC_Error &error)
{
    static char file[ABC_MAX_STRING_LENGTH + 1];
    static char function[ABC_MAX_STRING_LENGTH + 1];
    strncpy(file, error.szSourceFile, ABC_MAX_STRING_LENGTH);
    strncpy(function, error.szSourceFunc, ABC_MAX_STRING_LENGTH);
    file[ABC_MAX_STRING_LENGTH] = 0;
    function[ABC_MAX_STRING_LENGTH] = 0;

    return Status(error.code, error.szDescription, \
        file, function, error.nSourceLine); \
}

std::ostream &operator<<(std::ostream &output, const Status &s)
{
    output <<
        s.file() << ":" << s.line() << ": " << s.function() <<
        " returned error " << s.value() << " (" << s.message() << ")";
    return output;
}

} // namespace abcd
