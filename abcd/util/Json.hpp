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

tABC_CC ABC_UtilCreateArrayJSONObject(char   **aszValues,
                                      unsigned int count,
                                      const char   *szFieldName,
                                      json_t       **ppJSON_Data,
                                      tABC_Error   *pError);

tABC_CC ABC_UtilGetArrayValuesFromJSONString(const char *szJSON,
                                             const char *szFieldName,
                                             char       ***aszValues,
                                             unsigned int *pCount,
                                             tABC_Error *pError);

void *ABC_UtilJanssonSecureMalloc(size_t size);

void ABC_UtilJanssonSecureFree(void *ptr);

char *ABC_UtilStringFromJSONObject(const json_t *pJSON_Data, size_t flags);

} // namespace abcd

#endif
