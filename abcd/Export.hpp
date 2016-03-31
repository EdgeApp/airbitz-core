/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */
/**
 * @file
 * Functions for exporting wallet meta-data.
 */

#ifndef ABC_Export_h
#define ABC_Export_h

#include "util/Status.hpp"

namespace abcd {

tABC_CC ABC_ExportFormatCsv(tABC_TxInfo **pTransactions,
                            unsigned int iTransactionCount,
                            char **szCsvData,
                            tABC_Error *pError);

Status
exportFormatQBO(std::string &result, tABC_TxInfo **pTransactions,
                unsigned int iTransactionCount);

} // namespace abcd

#endif
