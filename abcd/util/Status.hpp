/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 */
#ifndef ABCD_UTIL_STATUS_HPP
#define ABCD_UTIL_STATUS_HPP

// We need tABC_CC and tABC_Error:
#include "../../src/ABC.h"
#include <ostream>
#include <string>

namespace abcd {

/**
 * Describes the results of calling a core function,
 * which can be either success or failure.
 */
class Status
{
public:
    /**
     * Constructs a success status.
     */
    Status();

    /**
     * Constructs an error status.
     */
    Status(tABC_CC value, std::string message,
        const char *file, const char *function, size_t line);

    // Read accessors:
    tABC_CC value()             const { return value_; }
    std::string message()       const { return message_; }
    std::string file()          const { return file_; }
    std::string function()      const { return function_; }
    size_t line()               const { return line_; }

    /**
     * Returns true if the status code represents success.
     */
    explicit operator bool() const { return value_ == ABC_CC_Ok; }

    /**
     * Unpacks this status into a tABC_Error structure.
     */
    void toError(tABC_Error &error);

    /**
     * Converts a tABC_Error structure into a Status.
     */
    static Status fromError(const tABC_Error &error);

private:
    // Error information:
    tABC_CC value_;
    std::string message_;

    // Error location:
    const char *file_;
    const char *function_;
    size_t line_;
};

std::ostream &operator<<(std::ostream &output, const Status &s);

/**
 * Constructs an error status using the current source location.
 */
#define ABC_ERROR(value, message) \
    Status(value, message, __FILE__, __FUNCTION__, __LINE__)

/**
 * Checks a status code, and returns if it represents an error.
 */
#define ABC_CHECK(f) \
    do { \
        Status s = (f); \
        if (!s) return s; \
    } while (false)

/**
 * Use when a new-style function calls an old-style tABC_Error function.
 */
#define ABC_CHECK_OLD(f) \
    do { \
        tABC_CC cc; \
        tABC_Error error; \
        error.code = ABC_CC_Ok; \
        cc = f; \
        if (ABC_CC_Ok != cc) \
            return Status::fromError(error); \
    } while (false)

/**
 * Use when an old-style function calls a new-style abcd::Status function.
 */
#define ABC_CHECK_NEW(f, pError) \
    do { \
        Status s = (f); \
        if (!s) { \
            s.toError(*pError); \
            cc = s.value(); \
            goto exit; \
        } \
    } while (false)

} // namespace abcd

#endif
