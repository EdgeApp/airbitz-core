/**
 * @file
 * AirBitz Utility function prototypes
 *
 *
 * @author Adam Harris
 * @version 1.0
 */

#ifndef ABC_Util_h
#define ABC_Util_h

#include <string.h>
#include <strings.h>
#include "ABC.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ABC_SET_ERR_CODE(err, set_code) \
    if (err != NULL) { \
        err->code = set_code; \
    }

#define ABC_CHECK_SYS(test, name) \
    if (!(test)) { \
        ABC_RET_ERROR(ABC_CC_SysError, "System function "name" failed."); \
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

#define ABC_CHECK_RET(err) \
    { \
        cc = err; \
        if (ABC_CC_Ok != cc) \
        { \
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

#define ABC_BUF(type)                       struct {  \
                                                    type *p;  \
                                                    type *end;  \
                                            }
#define ABC_BUF_SIZE(x)                     ((unsigned int) (((x).end)-((x).p)))
#define ABC_BUF_FREE(x)                     {  \
                                                free((x).p);  \
                                                (x).p = NULL;  \
                                                (x).end = NULL;  \
                                            }
#define ABC_BUF_CLEAR(x)                    {  \
                                                (x).p = NULL;  \
                                                (x).end = NULL;  \
                                            }
#define ABC_BUF_NULL                        { NULL, NULL }
#define ABC_BUF_NEW(buf, count)             { \
                                                (buf).p = calloc(count, sizeof(*((buf).p))); \
                                                (buf).end = (buf).p + (sizeof(*((buf).p)) * count); \
                                            }
#define ABC_BUF_PTR(x)                      ((x).p)
#define ABC_BUF_REALLOC(buf, count)         { \
                                                unsigned long __abc_buf_realloc_count__ = (sizeof(*((buf).p)) * count); \
                                                (buf).p = realloc((buf).p, __abc_buf_realloc_count__); \
                                                (buf).end = (buf).p +      __abc_buf_realloc_count__; \
                                            }
#define ABC_BUF_EXPAND(buf, count)          { \
                                                unsigned long __abc_buf_expand_count__ = (((buf).end)-((buf).p)) + (sizeof(*((buf).p)) * count); \
                                                (buf).p = realloc((buf).p, __abc_buf_expand_count__); \
                                                (buf).end = (buf).p +      __abc_buf_expand_count__; \
                                            }
#define ABC_BUF_APPEND_PTR(buf, ptr, count) { \
                                                unsigned long __abc_buf_append_resize__ = (unsigned long) (((buf).end)-((buf).p)) + (sizeof(*((buf).p)) * count); \
                                                (buf).p = realloc((buf).p, __abc_buf_append_resize__); \
                                                (buf).end = (buf).p +      __abc_buf_append_resize__; \
                                                memcpy((buf).end - count, ptr, count); \
                                            }
#define ABC_BUF_APPEND(dst, src)            { \
                                                unsigned long __abc_buf_append_size__ = (unsigned long) (((src).end)-((src).p)); \
                                                unsigned long __abc_buf_append_resize__ = (unsigned long) (((dst).end)-((dst).p)) + __abc_buf_append_size__ ; \
                                                (dst).p = realloc((dst).p, __abc_buf_append_resize__); \
                                                (dst).end = (dst).p +      __abc_buf_append_resize__; \
                                                memcpy((dst).end - __abc_buf_append_size__, (src).p,  __abc_buf_append_size__); \
                                            }
#define ABC_BUF_DUP(dst, src)               { \
                                                unsigned long __abc_buf_dup_size__ = (int) (((src).end)-((src).p)); \
                                                if (__abc_buf_dup_size__ > 0) \
                                                { \
                                                    (dst).p = malloc(__abc_buf_dup_size__); \
                                                    (dst).end = (dst).p + __abc_buf_dup_size__; \
                                                    memcpy((dst).p, (src).p, __abc_buf_dup_size__); \
                                                } \
                                            }
#define ABC_BUF_DUP_PTR(buf, ptr, size)     { \
                                                unsigned long __abc_buf_dup_size__ = (int) size; \
                                                (buf).p = malloc(__abc_buf_dup_size__); \
                                                (buf).end = (buf).p + __abc_buf_dup_size__; \
                                                memcpy((buf).p, ptr, __abc_buf_dup_size__); \
                                            }
#define ABC_BUF_SET(dst, src)               { (dst).p = (src).p; (dst).end = (src).end; }
#define ABC_BUF_SET_PTR(buf, ptr, size)     { (buf).p = ptr; (buf).end = ptr + size; }
#define ABC_BUF_ZERO(buf)                   { \
                                                if ((buf).p != NULL) \
                                                { \
                                                    memset ((buf).p, 0, (((buf).end)-((buf).p))); \
                                                } \
                                            }
#define ABC_CHECK_NULL_BUF(arg)             { ABC_CHECK_ASSERT(ABC_BUF_PTR(arg) != NULL, ABC_CC_NULLPtr, "NULL ABC_Buf pointer"); }
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

    tABC_CC ABC_UtilCreateArrayJSONString(char   **aszValues,
                                          unsigned int count,
                                          const char   *szFieldName,
                                          char         **pszJSON,
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


#ifdef __cplusplus
}
#endif

#endif
