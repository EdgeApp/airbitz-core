/**
 * @file
 * AirBitz Utility function prototypes
 *
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
 *
 *  @author See AUTHORS
 *  @version 1.0
 */

#ifndef ABC_Util_h
#define ABC_Util_h

#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <jansson.h>
#include "ABC.h"
#include "ABC_Debug.h"

#define CURRENCY_NUM_AUD                 36
#define CURRENCY_NUM_CAD                124
#define CURRENCY_NUM_CNY                156
#define CURRENCY_NUM_CUP                192
#define CURRENCY_NUM_HKD                344
#define CURRENCY_NUM_MXN                484
#define CURRENCY_NUM_NZD                554
#define CURRENCY_NUM_PHP                608
#define CURRENCY_NUM_GBP                826
#define CURRENCY_NUM_USD                840
#define CURRENCY_NUM_EUR                978

#define ABC_BITSTAMP "Bitstamp"
#define ABC_COINBASE "Coinbase"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
#define ABC_LOG_ERROR(code, err_string) \
    { \
        ABC_DebugLog("Error: %s, code: %d, func: %s, source: %s, line: %d", err_string, code, __FUNCTION__, __FILE__, __LINE__); \
    }
#else
    #define ABC_LOG_ERROR(code, err_string) { }
#endif

#define ABC_SET_ERR_CODE(err, set_code) \
    if (err != NULL) { \
        err->code = set_code; \
    }

#define ABC_CHECK_SYS(test, name) \
    if (!(test)) { \
        ABC_RET_ERROR(ABC_CC_SysError, "System function " name " failed."); \
    } else \

#define ABC_RET_ERROR(err, desc) \
    { \
        if (pError) \
        { \
            pError->code = err; \
            strcpy(pError->szDescription, desc); \
            strcpy(pError->szSourceFunc, __FUNCTION__); \
            strcpy(pError->szSourceFile, __FILE__); \
            pError->nSourceLine = __LINE__; \
        } \
        cc = err; \
        ABC_LOG_ERROR(cc, desc); \
        goto exit; \
    }

#define ABC_CHECK_ASSERT(assert, err, desc) \
    { \
        if (!(assert)) \
        { \
            ABC_RET_ERROR(err, desc); \
        } \
    } \

#define ABC_CHECK_NULL(arg) \
    { \
        ABC_CHECK_ASSERT(arg != NULL, ABC_CC_NULLPtr, "NULL pointer"); \
    } \

#define ABC_CHECK_NUMERIC(str, err, desc) \
    { \
        if (str) { \
            char *endstr = NULL; \
            strtol(str, &endstr, 10); \
            ABC_CHECK_ASSERT(*endstr == '\0', err, desc); \
        } \
    } \

#define ABC_CHECK_RET(err) \
    { \
        cc = err; \
        if (ABC_CC_Ok != cc) \
        { \
            ABC_LOG_ERROR(cc, #err); \
            goto exit; \
        } \
    }

#define ABC_PRINT_ERR(err) \
    { \
        if (err) \
        { \
            printf("Desc: %s, Func: %s, File: %s, Line: %d",  \
                    pError->szDescription, \
                    pError->szSourceFunc,\
                    pError->szSourceFile, \
                    pError->nSourceLine \
            ); \
        } \
        printf("\n"); \
    }

#define ABC_PRINT_ERR_CODE(err) \
    { \
        printf("ABC Error Code: %d, ", err); \
        ABC_PRINT_ERR(pError); \
    }

#define ABC_NEW(ptr, type) \
    { \
        ptr = (type*)calloc(1, sizeof(type)); \
        ABC_CHECK_ASSERT(ptr != NULL, ABC_CC_NULLPtr, "calloc failed (returned NULL)"); \
    }

#define ABC_STR_NEW(ptr, count) \
    { \
        ptr = (char*)calloc(count, sizeof(char)); \
        ABC_CHECK_ASSERT(ptr != NULL, ABC_CC_NULLPtr, "calloc failed (returned NULL)"); \
    }

#define ABC_ARRAY_NEW(ptr, count, type) \
    { \
        ptr = (type*)calloc(count, sizeof(type)); \
        ABC_CHECK_ASSERT(ptr != NULL, ABC_CC_NULLPtr, "calloc failed (returned NULL)"); \
    }

#define ABC_ARRAY_RESIZE(ptr, count, type) \
    { \
        ptr = (type*)realloc(ptr, (count)*sizeof(type)); \
        ABC_CHECK_ASSERT(ptr != NULL, ABC_CC_NULLPtr, "realloc failed (returned NULL)"); \
    }

#define ABC_STRLEN(string) (string == NULL ? 0 : strlen(string))

#define ABC_STRDUP(ptr, string) \
    { \
        if (string) {\
            ptr = strdup(string); \
            ABC_CHECK_ASSERT(ptr != NULL, ABC_CC_NULLPtr, "strdup failed (returned NULL)"); \
        }\
    }

#define ABC_FREE(ptr) \
    { \
        if (ptr != NULL) \
        { \
            free(ptr); \
            ptr = NULL; \
        } \
    }

#define ABC_FREE_STR(str) \
    { \
        if (str != NULL) \
        { \
            ABC_UtilGuaranteedMemset(str, 0, strlen(str)); \
            free(str); \
            str = NULL; \
        } \
    }

#define ABC_CLEAR_FREE(ptr, len) \
    { \
        if (ptr != NULL) \
        { \
            if ((len) > 0) \
            { \
                ABC_UtilGuaranteedMemset(ptr, 0, (len)); \
            } \
            free(ptr); \
            ptr = NULL; \
        } \
    }

#define ABC_SWAP(a, b) \
    { \
        unsigned char temp[sizeof(a) == sizeof(b) ? (signed)sizeof(a) : -1]; \
        memcpy(temp, &b, sizeof(a)); \
        memcpy(&b,   &a, sizeof(a)); \
        memcpy(&a, temp, sizeof(a)); \
    }

#define ABC_BUF(type)                       struct {  \
                                                    type *p;  \
                                                    type *end;  \
                                            }
#define ABC_BUF_SIZE(x)                     ((unsigned int) (((x).end)-((x).p)))
#define ABC_BUF_FREE(x)                     {  \
                                                if ((x).p != NULL) \
                                                { \
                                                    ABC_UtilGuaranteedMemset((x).p, 0, (((x).end)-((x).p))); \
                                                    free((x).p);  \
                                                } \
                                                (x).p = NULL;  \
                                                (x).end = NULL;  \
                                            }
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
#define ABC_BUF_REALLOC(buf, count)         { \
                                                unsigned long __abc_buf_realloc_count__ = (sizeof(*((buf).p)) * count); \
                                                (buf).p = (unsigned char*)realloc((buf).p, __abc_buf_realloc_count__); \
                                                (buf).end = (buf).p +      __abc_buf_realloc_count__; \
                                            }
#define ABC_BUF_EXPAND(buf, count)          { \
                                                unsigned long __abc_buf_expand_count__ = (((buf).end)-((buf).p)) + (sizeof(*((buf).p)) * count); \
                                                (buf).p = (unsigned char*)realloc((buf).p, __abc_buf_expand_count__); \
                                                (buf).end = (buf).p +      __abc_buf_expand_count__; \
                                            }
#define ABC_BUF_APPEND_PTR(buf, ptr, count) { \
                                                unsigned long __abc_buf_append_resize__ = (unsigned long) (((buf).end)-((buf).p)) + (sizeof(*((buf).p)) * count); \
                                                (buf).p = (unsigned char*)realloc((buf).p, __abc_buf_append_resize__); \
                                                (buf).end = (buf).p +      __abc_buf_append_resize__; \
                                                memcpy((buf).end - count, ptr, count); \
                                            }
#define ABC_BUF_APPEND(dst, src)            { \
                                                unsigned long __abc_buf_append_size__ = (unsigned long) (((src).end)-((src).p)); \
                                                unsigned long __abc_buf_append_resize__ = (unsigned long) (((dst).end)-((dst).p)) + __abc_buf_append_size__ ; \
                                                (dst).p = (unsigned char*)realloc((dst).p, __abc_buf_append_resize__); \
                                                (dst).end = (dst).p +      __abc_buf_append_resize__; \
                                                memcpy((dst).end - __abc_buf_append_size__, (src).p,  __abc_buf_append_size__); \
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
#define ABC_BUF_ZERO(buf)                   { \
                                                if ((buf).p != NULL) \
                                                { \
                                                    ABC_UtilGuaranteedMemset ((buf).p, 0, (((buf).end)-((buf).p))); \
                                                } \
                                            }
#define ABC_CHECK_NULL_BUF(arg)             { ABC_CHECK_ASSERT(ABC_BUF_PTR(arg) != NULL, ABC_CC_NULLPtr, "NULL ABC_Buf pointer"); }

#define ABC_BUF_SWAP(a, b)                  { \
                                                ABC_SWAP((a).p,   (b).p); \
                                                ABC_SWAP((a).end, (b).end); \
                                            }
/* example usage
{
    tABC_U08Buf mybuf = ABC_BUF_NULL; // declares buf and inits pointers to NULL
    ABC_BUF_NEW(mybuf, 10);           // allocates buffer with 10 elements
    int count = ABC_BUF_SIZE(myBuf);  // returns the count of elements in the buffer
    ABC_BUF_FREE(myBuf);              // frees the data in the buffer
}
*/

    typedef ABC_BUF(unsigned char) tABC_U08Buf;

    void ABC_UtilHexDump(const char *szDescription,
                         const unsigned char *pData,
                         unsigned int dataLength);

    void ABC_UtilHexDumpBuf(const char *szDescription,
                            tABC_U08Buf Buf);

    tABC_CC ABC_UtilCreateValueJSONString(const char *szValue,
                                          const char *szFieldName,
                                          char       **pszJSON,
                                          tABC_Error *pError);

    tABC_CC ABC_UtilCreateIntJSONString(int        value,
                                        const char *szFieldName,
                                        char       **pszJSON,
                                        tABC_Error *pError);

    tABC_CC ABC_UtilCreateArrayJSONObject(char   **aszValues,
                                          unsigned int count,
                                          const char   *szFieldName,
                                          json_t       **ppJSON_Data,
                                          tABC_Error   *pError);

    tABC_CC ABC_UtilCreateHexDataJSONString(const tABC_U08Buf Data,
                                            const char        *szFieldName,
                                            char              **pszJSON,
                                            tABC_Error        *pError);

    tABC_CC ABC_UtilGetStringValueFromJSONString(const char *szJSON,
                                                 const char *szFieldName,
                                                 char       **pszValue,
                                                 tABC_Error *pError);

    tABC_CC ABC_UtilGetIntValueFromJSONString(const char *szJSON,
                                              const char *szFieldName,
                                              int       *pValue,
                                              tABC_Error *pError);

    tABC_CC ABC_UtilGetArrayValuesFromJSONString(const char *szJSON,
                                                 const char *szFieldName,
                                                 char       ***aszValues,
                                                 unsigned int *pCount,
                                                 tABC_Error *pError);

    void ABC_UtilFreeStringArray(char **aszStrings,
                                 unsigned int count);

    void *ABC_UtilGuaranteedMemset(void *v, int c, size_t n);

    void *ABC_UtilJanssonSecureMalloc(size_t size);

    void ABC_UtilJanssonSecureFree(void *ptr);

    char *ABC_UtilStringFromJSONObject(const json_t *pJSON_Data, size_t flags);

#ifdef __cplusplus
}
#endif

#endif
