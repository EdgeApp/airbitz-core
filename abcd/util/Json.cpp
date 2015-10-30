/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Json.hpp"
#include "Util.hpp"

namespace abcd {

/**
 * This function is created so that we can override the free function of jansson so we can
 * clear memory on a free
 * reference: https://github.com/akheron/jansson/blob/master/doc/apiref.rst#id97
 */
void *ABC_UtilJanssonSecureMalloc(size_t size)
{
    /* Store the memory area size in the beginning of the block */
    char *ptr = (char*)malloc(size + 8);
    *((size_t *)ptr) = size;
    return ptr + 8;
}

/**
 * This function is created so that we can override the free function of jansson so we can
 * clear memory on a free
 * reference: https://github.com/akheron/jansson/blob/master/doc/apiref.rst#id97
 */
void ABC_UtilJanssonSecureFree(void *ptr)
{
    if (ptr != NULL)
    {
        size_t size;

        ptr = (char*)ptr - 8;
        size = *((size_t *)ptr);

        ABC_UtilGuaranteedMemset(ptr, 0, size + 8);
        free(ptr);
    }
}

} // namespace abcd
