/**
 * @file
 * AirBitz Export Data function prototypes.
 *
 * See LICENSE for copy, modification, and use permissions
 *
 * @author See AUTHORS
 * @version 1.0
 */

#ifndef ABC_Export_h
#define ABC_Export_h

#include "../src/ABC.h"

namespace abcd {

    tABC_CC ABC_ExportFormatCsv(tABC_TxInfo **pTransactions,
                                unsigned int iTransactionCount,
                                char **szCsvData,
                                tABC_Error *pError);


} // namespace abcd

#endif
