/**
 * @file
 * AirBitz utility function prototypes
 *
 * This file contains misc utility functions
 *
 * @author Adam Harris
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>
#include <jansson.h>
#include "ABC_Util.h"
#include "ABC_Crypto.h"

void ABC_UtilHexDumpBuf(const char *szDescription, 
                        tABC_U08Buf Buf)
{
    ABC_UtilHexDump(szDescription, ABC_BUF_PTR(Buf), ABC_BUF_SIZE(Buf));
}

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

// creates the json package with a single field and its value
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

    *pszJSON = json_dumps(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);

exit:
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

// creates the json package with a single field and its int value
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
    
    *pszJSON = json_dumps(pJSON_Root, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    
exit:
    if (pJSON_Root) json_decref(pJSON_Root);
    
    return cc;
}

tABC_CC ABC_UtilCreateArrayJSONString(char         **aszValues,
                                      unsigned int count,
                                      const char   *szFieldName,
                                      char         **pszJSON,
                                      tABC_Error   *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    json_t *jsonItems = NULL;
    json_t *jsonItemArray = NULL;
    char *szNewItems = NULL;
    
    ABC_CHECK_NULL(szFieldName);
    ABC_CHECK_NULL(pszJSON);
    
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
    
    *pszJSON = json_dumps(jsonItems, JSON_INDENT(4) | JSON_PRESERVE_ORDER);
    
exit:
    if (jsonItems)      json_decref(jsonItems);
    if (jsonItemArray)  json_decref(jsonItemArray);
    if (szNewItems)     free(szNewItems);
    
    return cc;
}

// creates the json package with a single field
// the field is encoded into hex and given the specified name
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
    if (szData_Hex) free(szData_Hex);
    
    return cc;
}


// gets the specified field from a json string
// the user is responsible for free'ing the value
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
    *pszValue = strdup(json_string_value(pJSON_Value));

exit:
    if (pJSON_Root) json_decref(pJSON_Root);

    return cc;
}

// gets the specified field from a json string
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

// gets the specified field from a json string
// the user is responsible for free'ing the array
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
        pArrayStrings = calloc(1, sizeof(char *) * *pCount);
        
        for (int i = 0; i < *pCount; i++)
        {
            json_t *pJSON_Elem = json_array_get(pJSON_Value, i);
            ABC_CHECK_ASSERT((pJSON_Elem && json_is_string(pJSON_Elem)), ABC_CC_JSONError, "Error parsing JSON string value");
            pArrayStrings[i] = strdup(json_string_value(pJSON_Elem));
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
    if (pArrayStrings) free(pArrayStrings);
    
    return cc;
}

// free's array of strings
// note: the strings are first zero'ed out before being freed
void ABC_UtilFreeStringArray(char **aszStrings,
                             unsigned int count)
{
    if ((aszStrings != NULL) && (count > 0))
    {
        for (int i = 0; i < count; i++)
        {
            free(aszStrings[i]);
            memset(aszStrings[i], 0, strlen(aszStrings[i]));
        }
        free(aszStrings);
    }
}


