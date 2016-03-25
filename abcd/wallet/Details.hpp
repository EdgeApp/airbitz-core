/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Helpers for dealing with Airbitz transaction metadata.
 */

#ifndef ABCD_WALLET_DETAILS_HPP
#define ABCD_WALLET_DETAILS_HPP

#include "../util/Status.hpp"
#include "../json/JsonObject.hpp"

namespace abcd {

/**
 * Free a TX details struct
 */
void
ABC_TxDetailsFree(tABC_TxDetails *pDetails);

/**
 * Decodes the transaction details data from a json transaction or address object
 *
 * @param ppInfo Pointer to store allocated meta info
 *               (it is the callers responsiblity to free this)
 */
tABC_CC
ABC_TxDetailsDecode(json_t *pJSON_Obj, tABC_TxDetails **ppDetails,
                    tABC_Error *pError);

/**
 * Encodes the transaction details data into the given json transaction object
 *
 * @param pJSON_Obj Pointer to the json object into which the details are stored.
 * @param pDetails  Pointer to the details to store in the json object.
 */
tABC_CC
ABC_TxDetailsEncode(json_t *pJSON_Obj, tABC_TxDetails *pDetails,
                    tABC_Error *pError);

} // namespace abcd

#endif
