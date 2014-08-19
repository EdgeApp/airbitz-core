/**
 * @file
 * AirBitz utility function prototypes
 *
 * This file contains misc utility functions
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

#include <stdio.h>
#include <string.h>
#include "ABC_Util.h"
#include "ABC_Crypto.h"

/**
 * Dumps the given buffer to stdio in od -c format
 */
void ABC_UtilHexDumpBuf(const char *szDescription,
                        tABC_U08Buf Buf)
{
    ABC_UtilHexDump(szDescription, ABC_BUF_PTR(Buf), ABC_BUF_SIZE(Buf));
}

/**
 * Dumps the given data to stdio in od -c format
 */
void ABC_UtilHexDump(const char *szDescription,
                     const unsigned char *pData,
                     unsigned int dataLength)
{
    int i;
    unsigned char buff[17];
    unsigned char *pc = (unsigned char *) pData;

    // Output description if given.
    if (szDescription != NULL)
        printf ("%s:\n", szDescription);

    // Process every byte in the data.
    for (i = 0; i < dataLength; i++) {
        // Multiple of 16 means new line (with line offset).

        if ((i % 16) == 0) {
            // Just don't print ASCII for the zeroth line.
            if (i != 0)
                printf ("  %s\n", buff);

            // Output the offset.
            printf ("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf (" %02x", pc[i]);

        // And store a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e))
            buff[i % 16] = '.';
        else
            buff[i % 16] = pc[i];
        buff[(i % 16) + 1] = '\0';
    }

    // Pad out last line if not exactly 16 characters.
    while ((i % 16) != 0) {
        printf ("   ");
        i++;
    }

    // And print the final ASCII bit.
    printf ("  %s\n", buff);
}

/**
 * Creates the json package with a single field and its value
 */
tABC_CC ABC_UtilCreateValueJSONString(const char *szValue,
                                      const char *szFieldName,
                                      char       **pszJSON,
                                      tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL(szValue);
    ABC_CHECK_NULL(szFieldName);
    ABC_CHECK_NULL(pszJSON);

    // create the jansson object
    pJSON_Root = json_pack("{ss}", szFieldName, szValue);

    *pszJSON = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

exit:
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

/**
 * Creates the json package with a single field and its int value
 */
tABC_CC ABC_UtilCreateIntJSONString(int        value,
                                    const char *szFieldName,
                                    char       **pszJSON,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Root = NULL;

    ABC_CHECK_NULL(szFieldName);
    ABC_CHECK_NULL(pszJSON);

    // create the jansson object
    pJSON_Root = json_pack("{si}", szFieldName, value);

    *pszJSON = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

exit:
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

/**
 *  Creates a JSON string representing an array of values with their name
 */
tABC_CC ABC_UtilCreateArrayJSONObject(char   **aszValues,
                                      unsigned int count,
                                      const char   *szFieldName,
                                      json_t       **ppJSON_Data,
                                      tABC_Error   *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *jsonItems = NULL;
    json_t *jsonItemArray = NULL;

    ABC_CHECK_NULL(szFieldName);
    ABC_CHECK_NULL(ppJSON_Data);

    // create the json object that will be our questions
    jsonItems = json_object();
    jsonItemArray = json_array();

    // if there are values
    if ((count > 0) && (aszValues != NULL))
    {
        for (int i = 0; i < count; i++)
        {
            json_array_append_new(jsonItemArray, json_string(aszValues[i]));
        }
    }

    // set our final json for the array element
    json_object_set(jsonItems, szFieldName, jsonItemArray);

    *ppJSON_Data = jsonItems;
    jsonItems = NULL;

exit:
    if (jsonItems)      json_decref(jsonItems);
    if (jsonItemArray)  json_decref(jsonItemArray);

    return cc;
}

/**
 * Creates the json package with a single field
 * the field is encoded into hex and given the specified name
 */
tABC_CC ABC_UtilCreateHexDataJSONString(const tABC_U08Buf Data,
                                        const char        *szFieldName,
                                        char              **pszJSON,
                                        tABC_Error        *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    char   *szData_Hex = NULL;

    ABC_CHECK_NULL_BUF(Data);
    ABC_CHECK_NULL(szFieldName);
    ABC_CHECK_NULL(pszJSON);

    // encode the Data into a Hex string
    ABC_CHECK_RET(ABC_CryptoHexEncode(Data, &szData_Hex, pError));

    // create the json
    ABC_CHECK_RET(ABC_UtilCreateValueJSONString(szData_Hex, szFieldName, pszJSON, pError));

exit:
    ABC_FREE_STR(szData_Hex);

    return cc;
}


/**
 * Gets the specified field from a json string
 * the user is responsible for free'ing the value
 */
tABC_CC ABC_UtilGetStringValueFromJSONString(const char *szJSON,
                                             const char *szFieldName,
                                             char       **pszValue,
                                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Root = NULL;
    json_t *pJSON_Value = NULL;

    ABC_CHECK_NULL(szJSON);
    ABC_CHECK_NULL(szFieldName);
    ABC_CHECK_NULL(pszValue);

    // decode the object
    json_error_t error;
    pJSON_Root = json_loads(szJSON, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // get the field
    pJSON_Value = json_object_get(pJSON_Root, szFieldName);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_string(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON string value");
    ABC_STRDUP(*pszValue, json_string_value(pJSON_Value));

exit:
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

/**
 * Gets the specified field from a json string
 */
tABC_CC ABC_UtilGetIntValueFromJSONString(const char *szJSON,
                                          const char *szFieldName,
                                          int       *pValue,
                                          tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Root = NULL;
    json_t *pJSON_Value = NULL;

    ABC_CHECK_NULL(szJSON);
    ABC_CHECK_NULL(szFieldName);
    ABC_CHECK_NULL(pValue);

    // decode the object
    json_error_t error;
    pJSON_Root = json_loads(szJSON, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // get the field
    pJSON_Value = json_object_get(pJSON_Root, szFieldName);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_number(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON int value");
    *pValue = (int) json_integer_value(pJSON_Value);

exit:
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

/**
 * Gets the specified field from a json string
 * the user is responsible for free'ing the array
 */
tABC_CC ABC_UtilGetArrayValuesFromJSONString(const char *szJSON,
                                             const char *szFieldName,
                                             char       ***aszValues,
                                             unsigned int *pCount,
                                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    json_t *pJSON_Root = NULL;
    json_t *pJSON_Value = NULL;
    char **pArrayStrings = NULL;

    ABC_CHECK_NULL(szJSON);
    ABC_CHECK_NULL(szFieldName);
    ABC_CHECK_NULL(aszValues);
    *aszValues = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    // decode the object
    json_error_t error;
    pJSON_Root = json_loads(szJSON, 0, &error);
    ABC_CHECK_ASSERT(pJSON_Root != NULL, ABC_CC_JSONError, "Error parsing JSON");
    ABC_CHECK_ASSERT(json_is_object(pJSON_Root), ABC_CC_JSONError, "Error parsing JSON");

    // get the field
    pJSON_Value = json_object_get(pJSON_Root, szFieldName);
    ABC_CHECK_ASSERT((pJSON_Value && json_is_array(pJSON_Value)), ABC_CC_JSONError, "Error parsing JSON array value");

    // get the number of elements in the array
    *pCount = (int) json_array_size(pJSON_Value);

    if (*pCount > 0)
    {
        ABC_ALLOC(pArrayStrings, sizeof(char *) * *pCount);

        for (int i = 0; i < *pCount; i++)
        {
            json_t *pJSON_Elem = json_array_get(pJSON_Value, i);
            ABC_CHECK_ASSERT((pJSON_Elem && json_is_string(pJSON_Elem)), ABC_CC_JSONError, "Error parsing JSON string value");
            ABC_STRDUP(pArrayStrings[i], json_string_value(pJSON_Elem));
        }

        *aszValues = pArrayStrings;
        pArrayStrings = NULL;
    }
    else
    {
        *aszValues = NULL;
    }


exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    ABC_CLEAR_FREE(pArrayStrings, sizeof(char *) * *pCount);

    return cc;
}

/**
 * Free's array of strings
 * note: the strings are first zero'ed out before being freed
 */
void ABC_UtilFreeStringArray(char **aszStrings,
                             unsigned int count)
{
    if ((aszStrings != NULL) && (count > 0))
    {
        for (int i = 0; i < count; i++)
        {
            ABC_FREE_STR(aszStrings[i]);
        }
        ABC_FREE(aszStrings);
    }
}

/**
 * For security reasons, it is important that we always make sure memory is set the way we expect
 * this function should ensure that
 * reference: http://www.dwheeler.com/secure-programs/Secure-Programs-HOWTO/protect-secrets.html
 */
void *ABC_UtilGuaranteedMemset(void *v, int c, size_t n)
{
    if (v)
    {
        volatile char *p = v;
        while (n--)
        {
            *p++ = c;
        }
    }

    return v;
}

/**
 * This function is created so that we can override the free function of jansson so we can
 * clear memory on a free
 * reference: https://github.com/akheron/jansson/blob/master/doc/apiref.rst#id97
 */
void *ABC_UtilJanssonSecureMalloc(size_t size)
{
    /* Store the memory area size in the beginning of the block */
    void *ptr = malloc(size + 8);
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

        ptr -= 8;
        size = *((size_t *)ptr);

        ABC_UtilGuaranteedMemset(ptr, 0, size + 8);
        free(ptr);
    }
}

/**
 * Generates the JSON string for a jansson object.
 * Note: the given string is allocated and must be free'd by the caller
 *
 * The reason for this function is because we have overridden the alloc and
 * free for jansson so that we can memset it on free.
 * In order to do this, the size of the data must be stored in the beginning
 * of the allocated data.
 * Therefore, any memory allocated by jansson must be free'd with the matching
 * free function.
 * This includes json_dumps and we don't want to make users of strings that are
 * generated via json_dumps to have to know this. They should be able to treat
 * them as regular strings.
 */
char *ABC_UtilStringFromJSONObject(const json_t *pJSON_Data, size_t flags)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Error error;
    tABC_Error *pError = &error;
    char *strJanssonJSON = NULL;
    char *strJSON = NULL;

    strJanssonJSON = json_dumps(pJSON_Data, flags);

    if (strJanssonJSON != NULL)
    {
        ABC_STRDUP(strJSON, strJanssonJSON);
    }

exit:
    ABC_UtilJanssonSecureFree(strJanssonJSON);

    return strJSON;
}

