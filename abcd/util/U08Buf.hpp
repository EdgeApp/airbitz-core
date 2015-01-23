/*
 * Copyright (c) 2014, AirBitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file An old C-style data slice and associated helpers.
 */

#ifndef ABCD_UTIL_U08BUF_H
#define ABCD_UTIL_U08BUF_H

// TODO: Eliminate macros, so we don't need to pull this in:
#include "Util.hpp"

namespace abcd {

/**
 * A slice of raw data.
 */
struct U08Buf
{
    unsigned char *p;
    unsigned char *end;
};

/**
 * Frees the buffer contents and sets the pointers to null.
 */
void U08BufFree(U08Buf &self);

/**
 * A data slice that can automatically free itself.
 */
struct AutoU08Buf:
    public U08Buf
{
    ~AutoU08Buf()
    {
        U08BufFree(*this);
    }

    AutoU08Buf()
    {
        p = nullptr;
        end = nullptr;
    }
};

#define ABC_BUF_SIZE(x)                     ((unsigned int) (((x).end)-((x).p)))
#define ABC_BUF_CLEAR(x)                    {  \
                                                (x).p = NULL;  \
                                                (x).end = NULL;  \
                                            }
#define ABC_BUF_NULL                        { NULL, NULL }
#define ABC_BUF_NEW(buf, count)             { \
                                                (buf).p = (unsigned char*)calloc(count, sizeof(*((buf).p))); \
                                                (buf).end = (buf).p + (sizeof(*((buf).p)) * count); \
                                            }
#define ABC_BUF_PTR(x)                      ((x).p)
#define ABC_BUF_APPEND_PTR(buf, ptr, count) { \
                                                unsigned long __abc_buf_append_resize__ = (unsigned long) (((buf).end)-((buf).p)) + (sizeof(*((buf).p)) * count); \
                                                (buf).p = (unsigned char*)realloc((buf).p, __abc_buf_append_resize__); \
                                                (buf).end = (buf).p +      __abc_buf_append_resize__; \
                                                memcpy((buf).end - count, ptr, count); \
                                            }
#define ABC_BUF_DUP(dst, src)               { \
                                                unsigned long __abc_buf_dup_size__ = (int) (((src).end)-((src).p)); \
                                                if (__abc_buf_dup_size__ > 0) \
                                                { \
                                                    (dst).p = (unsigned char*)malloc(__abc_buf_dup_size__); \
                                                    (dst).end = (dst).p + __abc_buf_dup_size__; \
                                                    memcpy((dst).p, (src).p, __abc_buf_dup_size__); \
                                                } \
                                            }
#define ABC_BUF_STRCAT(dst, a, b)           { \
                                                unsigned long __abc_buf_a_size__ = strlen(a); \
                                                unsigned long __abc_buf_b_size__ = strlen(b); \
                                                (dst).p = (unsigned char*)malloc(__abc_buf_a_size__ + __abc_buf_b_size__); \
                                                (dst).end = (dst).p + __abc_buf_a_size__ + __abc_buf_b_size__; \
                                                memcpy((dst).p, (a), __abc_buf_a_size__); \
                                                memcpy((dst).p + __abc_buf_a_size__, (b), __abc_buf_b_size__); \
                                            }
#define ABC_BUF_DUP_PTR(buf, ptr, size)     { \
                                                unsigned long __abc_buf_dup_size__ = (int) size; \
                                                (buf).p = (unsigned char*)malloc(__abc_buf_dup_size__); \
                                                (buf).end = (buf).p + __abc_buf_dup_size__; \
                                                memcpy((buf).p, ptr, __abc_buf_dup_size__); \
                                            }
#define ABC_BUF_SET(dst, src)               { (dst).p = (src).p; (dst).end = (src).end; }
#define ABC_BUF_SET_PTR(buf, ptr, size)     { (buf).p = ptr; (buf).end = ptr + size; }
#define ABC_CHECK_NULL_BUF(arg)             { ABC_CHECK_ASSERT(ABC_BUF_PTR(arg) != NULL, ABC_CC_NULLPtr, "NULL ABC_Buf pointer"); }

/* example usage
{
    AutoU08Buf mybuf;                 // declares buf and inits pointers to NULL
    ABC_BUF_NEW(mybuf, 10);           // allocates buffer with 10 elements
    int count = ABC_BUF_SIZE(myBuf);  // returns the count of elements in the buffer
}
*/

// Compatibility wrappers:
typedef U08Buf tABC_U08Buf;
#define ABC_BUF_FREE(x) U08BufFree(x);

} // namespace abcd

#endif
