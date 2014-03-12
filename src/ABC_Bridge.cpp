/**
 * @file
 * AirBitz C++ wrappers.
 *
 * This file contains a bridge between the plain C code in the core, and
 * the C++ code in libbitcoin and friends.
 *
 * @author William Swanson
 * @version 0.1
 */

#include "ABC_Bridge.h"
#include "ABC_Util.h"

/**
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param szURI     URI to parse
 * @param ppInfo    Pointer to location to store allocated info struct.
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_BridgeParseBitcoinURI(const char *szURI,
                            tABC_BitcoinURIInfo **ppInfo,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_BitcoinURIInfo *pInfo = NULL;

    ABC_CHECK_NULL(szURI);
    ABC_CHECK_ASSERT(strlen(szURI) > 0, ABC_CC_Error, "No URI provided");
    ABC_CHECK_NULL(ppInfo);
    *ppInfo = NULL;

    // allocate initial struct
    ABC_ALLOC_ARRAY(pInfo, 1, tABC_BitcoinURIInfo);

    // TODO: parse the elements and store them in the pInfo struct
    // Note: if a given member (e.g., label) doesn't exist, a blank string should still be allocated

    // assign created info struct
    *ppInfo = pInfo;
    pInfo = NULL; // do this so we don't free below what we just gave the caller

exit:
    ABC_BridgeFreeURIInfo(pInfo);

    return cc;
}

/**
 * Frees the memory allocated by ABC_BridgeParseBitcoinURI.
 *
 * @param pInfo Pointer to allocated info struct.
 */
void ABC_BridgeFreeURIInfo(tABC_BitcoinURIInfo *pInfo)
{
    if (pInfo != NULL)
    {
        ABC_FREE_STR(pInfo->szLabel);

        ABC_FREE_STR(pInfo->szAddress);

        ABC_FREE_STR(pInfo->szMessage);

        ABC_CLEAR_FREE(pInfo, sizeof(tABC_BitcoinURIInfo));
    }
}
