/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file Helper functions for working with the Jansson library.
 */

#ifndef ABCD_UTIL_JSON_H
#define ABCD_UTIL_JSON_H

#include "../../src/ABC.h"
#include <jansson.h>

namespace abcd {

void *ABC_UtilJanssonSecureMalloc(size_t size);

void ABC_UtilJanssonSecureFree(void *ptr);

} // namespace abcd

#endif
