/*
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
 */
/**
 * @file
 * General-purpose data types and utility macros.
 */

#ifndef ABC_Util_h
#define ABC_Util_h

#include "../../src/ABC.h"
#include "Debug.hpp"
#include <string.h>
#include <strings.h>
#include <stdlib.h>

namespace abcd {

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
        ptr = strdup(string); \
        ABC_CHECK_ASSERT(ptr != NULL, ABC_CC_NULLPtr, "strdup failed (returned NULL)"); \
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

void ABC_UtilFreeStringArray(char **aszStrings,
                             unsigned int count);

void *ABC_UtilGuaranteedMemset(void *v, int c, size_t n);

} // namespace abcd

#endif
