/*
 *  Copyright (c) 2015, AirBitz, Inc.
 *  All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#ifndef ABC_UTIL_DEBUG_HPP
#define ABC_UTIL_DEBUG_HPP

#include "Status.hpp"
#include "Data.hpp"

namespace abcd {

Status
debugInitialize();

void
debugTerminate();

DataChunk
debugLogLoad();

void ABC_DebugLog(const char *format, ...);

} // namespace abcd

#endif
