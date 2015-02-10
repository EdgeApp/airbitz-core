/*
 * Copyright (c) 2015, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef UTIL_OTP_HPP
#define UTIL_OTP_HPP

#include "../src/ABC.h"
#include "../abcd/util/Status.hpp"

abcd::Status otpKeyGet(int argc, char *argv[]);
abcd::Status otpKeySet(int argc, char *argv[]);
abcd::Status otpKeyRemove(int argc, char *argv[]);

abcd::Status otpAuthGet(int argc, char *argv[]);
abcd::Status otpAuthSet(int argc, char *argv[]);
abcd::Status otpAuthRemove(int argc, char *argv[]);

abcd::Status otpResetGet(int argc, char *argv[]);
abcd::Status otpResetSet(int argc, char *argv[]);
abcd::Status otpResetRemove(int argc, char *argv[]);

#endif
