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

// creates the json package with a single field and it's value
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
