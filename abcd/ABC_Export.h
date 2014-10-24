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

#include "ABC.h"
#include "util/ABC_Util.h"

#ifdef __cplusplus
extern "C" {
#endif

    tABC_CC ABC_ExportFormatCsv(tABC_TxInfo **pTransactions,
                                unsigned int iTransactionCount,
                                char **szCsvData,
                                tABC_Error *pError);


#ifdef __cplusplus
}
#endif

#endif
