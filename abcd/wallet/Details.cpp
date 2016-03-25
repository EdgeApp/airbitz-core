/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Details.hpp"
#include "../util/Util.hpp"

namespace abcd {

#define JSON_DETAILS_FIELD                      "meta"
#define JSON_AMOUNT_SATOSHI_FIELD               "amountSatoshi"
#define JSON_AMOUNT_AIRBITZ_FEE_SATOSHI_FIELD   "amountFeeAirBitzSatoshi"
#define JSON_AMOUNT_MINERS_FEE_SATOSHI_FIELD    "amountFeeMinersSatoshi"

#define JSON_TX_AMOUNT_CURRENCY_FIELD           "amountCurrency"
#define JSON_TX_NAME_FIELD                      "name"
#define JSON_TX_BIZID_FIELD                     "bizId"
#define JSON_TX_CATEGORY_FIELD                  "category"
#define JSON_TX_NOTES_FIELD                     "notes"
#define JSON_TX_ATTRIBUTES_FIELD                "attributes"

void
ABC_TxDetailsFree(tABC_TxDetails *pDetails)
{
    if (pDetails)
    {
        ABC_FREE_STR(pDetails->szName);
        ABC_FREE_STR(pDetails->szCategory);
        ABC_FREE_STR(pDetails->szNotes);
        ABC_CLEAR_FREE(pDetails, sizeof(tABC_TxDetails));
    }
}

tABC_CC
ABC_TxDetailsDecode(json_t *pJSON_Obj, tABC_TxDetails **ppDetails,
                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    AutoFree<tABC_TxDetails, ABC_TxDetailsFree>
    pDetails(structAlloc<tABC_TxDetails>());
    json_t *jsonDetails = NULL;
    json_t *jsonVal = NULL;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(ppDetails);
    *ppDetails = NULL;

    // get the details object
    jsonDetails = json_object_get(pJSON_Obj, JSON_DETAILS_FIELD);
    ABC_CHECK_ASSERT((jsonDetails
                      && json_is_object(jsonDetails)), ABC_CC_JSONError,
                     "Error parsing JSON details package - missing meta data (details)");

    // get the satoshi field
    jsonVal = json_object_get(jsonDetails, JSON_AMOUNT_SATOSHI_FIELD);
    ABC_CHECK_ASSERT((jsonVal
                      && json_is_integer(jsonVal)), ABC_CC_JSONError,
                     "Error parsing JSON details package - missing satoshi amount");
    pDetails->amountSatoshi = json_integer_value(jsonVal);

    // get the airbitz fees satoshi field
    jsonVal = json_object_get(jsonDetails, JSON_AMOUNT_AIRBITZ_FEE_SATOSHI_FIELD);
    if (jsonVal)
    {
        ABC_CHECK_ASSERT(json_is_integer(jsonVal), ABC_CC_JSONError,
                         "Error parsing JSON details package - malformed airbitz fees field");
        pDetails->amountFeesAirbitzSatoshi = json_integer_value(jsonVal);
    }

    // get the miners fees satoshi field
    jsonVal = json_object_get(jsonDetails, JSON_AMOUNT_MINERS_FEE_SATOSHI_FIELD);
    if (jsonVal)
    {
        ABC_CHECK_ASSERT(json_is_integer(jsonVal), ABC_CC_JSONError,
                         "Error parsing JSON details package - malformed miners fees field");
        pDetails->amountFeesMinersSatoshi = json_integer_value(jsonVal);
    }

    // get the currency field
    jsonVal = json_object_get(jsonDetails, JSON_TX_AMOUNT_CURRENCY_FIELD);
    ABC_CHECK_ASSERT((jsonVal
                      && json_is_real(jsonVal)), ABC_CC_JSONError,
                     "Error parsing JSON details package - missing currency amount");
    pDetails->amountCurrency = json_real_value(jsonVal);

    // get the name field
    jsonVal = json_object_get(jsonDetails, JSON_TX_NAME_FIELD);
    ABC_CHECK_ASSERT((jsonVal
                      && json_is_string(jsonVal)), ABC_CC_JSONError,
                     "Error parsing JSON details package - missing name");
    pDetails->szName = stringCopy(json_string_value(jsonVal));

    // get the business-directory id field
    jsonVal = json_object_get(jsonDetails, JSON_TX_BIZID_FIELD);
    if (jsonVal)
    {
        ABC_CHECK_ASSERT(json_is_integer(jsonVal), ABC_CC_JSONError,
                         "Error parsing JSON details package - malformed directory bizId field");
        pDetails->bizId = json_integer_value(jsonVal);
    }

    // get the category field
    jsonVal = json_object_get(jsonDetails, JSON_TX_CATEGORY_FIELD);
    ABC_CHECK_ASSERT((jsonVal
                      && json_is_string(jsonVal)), ABC_CC_JSONError,
                     "Error parsing JSON details package - missing category");
    pDetails->szCategory = stringCopy(json_string_value(jsonVal));

    // get the notes field
    jsonVal = json_object_get(jsonDetails, JSON_TX_NOTES_FIELD);
    ABC_CHECK_ASSERT((jsonVal
                      && json_is_string(jsonVal)), ABC_CC_JSONError,
                     "Error parsing JSON details package - missing notes");
    pDetails->szNotes = stringCopy(json_string_value(jsonVal));

    // get the attributes field
    jsonVal = json_object_get(jsonDetails, JSON_TX_ATTRIBUTES_FIELD);
    ABC_CHECK_ASSERT((jsonVal
                      && json_is_integer(jsonVal)), ABC_CC_JSONError,
                     "Error parsing JSON details package - missing attributes");
    pDetails->attributes = (unsigned int) json_integer_value(jsonVal);

    // assign final result
    *ppDetails = pDetails.release();

exit:
    return cc;
}

tABC_CC
ABC_TxDetailsEncode(json_t *pJSON_Obj, tABC_TxDetails *pDetails,
                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    json_t *pJSON_Details = NULL;
    int retVal = 0;

    ABC_CHECK_NULL(pJSON_Obj);
    ABC_CHECK_NULL(pDetails);

    // create the details object
    pJSON_Details = json_object();

    // add the satoshi field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_AMOUNT_SATOSHI_FIELD,
                                 json_integer(pDetails->amountSatoshi));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the airbitz fees satoshi field to the details object
    retVal = json_object_set_new(pJSON_Details,
                                 JSON_AMOUNT_AIRBITZ_FEE_SATOSHI_FIELD,
                                 json_integer(pDetails->amountFeesAirbitzSatoshi));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the miners fees satoshi field to the details object
    retVal = json_object_set_new(pJSON_Details,
                                 JSON_AMOUNT_MINERS_FEE_SATOSHI_FIELD,
                                 json_integer(pDetails->amountFeesMinersSatoshi));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the currency field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_AMOUNT_CURRENCY_FIELD,
                                 json_real(pDetails->amountCurrency));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the name field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_NAME_FIELD,
                                 json_string(pDetails->szName));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the business-directory id field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_BIZID_FIELD,
                                 json_integer(pDetails->bizId));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the category field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_CATEGORY_FIELD,
                                 json_string(pDetails->szCategory));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the notes field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_NOTES_FIELD,
                                 json_string(pDetails->szNotes));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the attributes field to the details object
    retVal = json_object_set_new(pJSON_Details, JSON_TX_ATTRIBUTES_FIELD,
                                 json_integer(pDetails->attributes));
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

    // add the details object to the master object
    retVal = json_object_set(pJSON_Obj, JSON_DETAILS_FIELD, pJSON_Details);
    ABC_CHECK_ASSERT(retVal == 0, ABC_CC_JSONError, "Could not encode JSON value");

exit:
    if (pJSON_Details) json_decref(pJSON_Details);

    return cc;
}

} // namespace abcd
