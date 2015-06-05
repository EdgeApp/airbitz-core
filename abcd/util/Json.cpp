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
        for (unsigned i = 0; i < count; i++)
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
        ABC_ARRAY_NEW(pArrayStrings, *pCount, char*);

        for (unsigned i = 0; i < *pCount; i++)
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

    ABC_CHECK_NULL(pJSON_Data);
    strJanssonJSON = json_dumps(pJSON_Data, flags);

    if (strJanssonJSON != NULL)
    {
        ABC_STRDUP(strJSON, strJanssonJSON);
    }

exit:
    ABC_UtilJanssonSecureFree(strJanssonJSON);

    return strJSON;
}

} // namespace abcd
