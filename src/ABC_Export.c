//
//  export.c
/**
 * @file
 * AirBitz file-sync functions prototypes.
 *
 * See LICENSE for copy, modification, and use permissions
 *
 * @author See AUTHORS
 * @version 1.0
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "ABC_Export.h"
#include "ABC_Util.h"
#include "csv.h"


tABC_CC ABC_ExportGenerateHeader(char **szCsvRec, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char **out = szCsvRec;

    /* header */
    char *szTimeCreation = "DATE";
    char *szName = "PAYEE_PAYER_NAME"; /* payee or payer */    
    char *szAmtBTC = "AMT_BTC";
    char *szCurrency = "USD";
    char *szCategory = "CATEGORY";
    char *szNotes = "NOTES";
    char *szAmtAirbitzBTC = "AMT_BTC_FEES_AB";
    char *szAmtFeesMinersBTC = "AMT_BTC_FEES_MINERS";
//    char *szInputAddresses = "IN_ADDRESSES";
//    char *szOutputAddresses = "OUT_ADDRESSES";
    char *szCsvId = "TXID";
    char *szCsvMaleableId = "MTXID";

    ABC_CHECK_NULL(szCsvRec);

    /* Allocated enough space for the header descriptions */
    ABC_ALLOC(*szCsvRec, ABC_CSV_MAX_REC_SZ);
    
    /* build the entire record */
    snprintf(*out, ABC_CSV_MAX_FLD_SZ,
//             "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
             "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
                szTimeCreation,
                szName, 
                szAmtBTC, 
                szCurrency,
                szCategory,
                szNotes,
                szAmtAirbitzBTC,
                szAmtFeesMinersBTC,
//                szInputAddresses,
//                szOutputAddresses
                szCsvId, 
                szCsvMaleableId, 
                ABC_CSV_REC_TERM_NAME);
exit:
    return cc;
}

tABC_CC ABC_ExportGenerateRecord(tABC_TxInfo *data, char **szCsvRec, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    tABC_TxInfo *pData = data;
    char **out = szCsvRec;

    /* header */
    char *szTimeCreation;
    char *szName; /* payee or payer */    
    char *szAmtBTC;
    char *szCurrency;
    char *szCategory;
    char *szNotes;
    char *szAmtAirbitzBTC;
    char *szAmtFeesMinersBTC;
    char *szInputAddresses;
    char *szOutputAddresses;
    char *szCsvId;
    char *szCsvMaleableId;

    char *pFormatted = NULL;

    tABC_CSV tmpCsvVar;

    ABC_CHECK_NULL(data);

    ABC_CSV_INIT2(tmpCsvVar, pData->timeCreation, PRIu64);
    ABC_CSV_FMT(tmpCsvVar, szTimeCreation);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szName);
    ABC_CSV_FMT(tmpCsvVar, szName);

    ABC_CHECK_RET(ABC_FormatAmount(pData->pDetails->amountSatoshi,&pFormatted, ABC_BITCOIN_DECIMAL_PLACES, pError));
    ABC_CSV_INIT(tmpCsvVar, pFormatted);
    ABC_CSV_FMT(tmpCsvVar, szAmtBTC);

    ABC_CSV_INIT2(tmpCsvVar, pData->pDetails->amountCurrency, "0.2f");
    ABC_CSV_FMT(tmpCsvVar, szCurrency);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szCategory);
    ABC_CSV_FMT(tmpCsvVar, szCategory);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szNotes);
    ABC_CSV_FMT(tmpCsvVar, szNotes);

    ABC_CHECK_RET(ABC_FormatAmount(pData->pDetails->amountFeesAirbitzSatoshi,&pFormatted, ABC_BITCOIN_DECIMAL_PLACES, pError));
    ABC_CSV_INIT(tmpCsvVar, pFormatted);
    ABC_CSV_FMT(tmpCsvVar, szAmtAirbitzBTC);

    ABC_CHECK_RET(ABC_FormatAmount(pData->pDetails->amountFeesMinersSatoshi,&pFormatted, ABC_BITCOIN_DECIMAL_PLACES, pError));
    ABC_CSV_INIT(tmpCsvVar, pFormatted);
    ABC_CSV_FMT(tmpCsvVar, szAmtFeesMinersBTC);

    ABC_CSV_INIT(tmpCsvVar, data->szID);
    ABC_CSV_FMT(tmpCsvVar, szCsvId);

    ABC_CSV_INIT(tmpCsvVar, data->szMalleableTxId);
    ABC_CSV_FMT(tmpCsvVar, szCsvMaleableId);

    /* Allocated enough space for one CSV REC - Determined by adding all field when quoted */
    ABC_ALLOC(*szCsvRec, ABC_CSV_MAX_REC_SZ+1);
    
    /* build the entire record */
    snprintf(*out, ABC_CSV_MAX_FLD_SZ,
             "%s%s%s%s%s%s%s%s%s%s%s",
                szTimeCreation,
                szName, 
                szAmtBTC, 
                szCurrency,
                szCategory,
                szNotes,
                szAmtAirbitzBTC,
                szAmtFeesMinersBTC,
//                szInputAddresses,
//                szOutputAddresses
                szCsvId, 
                szCsvMaleableId, 
                ABC_CSV_REC_TERM_VALUE);

    ABC_FREE(szTimeCreation);
    ABC_FREE(szName);
    ABC_FREE(szAmtBTC);
    ABC_FREE(szCurrency);
    ABC_FREE(szCategory);
    ABC_FREE(szNotes);
    ABC_FREE(szAmtAirbitzBTC);
    ABC_FREE(szAmtFeesMinersBTC);
    ABC_FREE(szCsvId);
    ABC_FREE(szCsvMaleableId);

    
exit:
    return cc;
}

tABC_CC ABC_ExportFormatCsv(const char *szUserName,
                            const char *szPassword,
                            tABC_TxInfo **pTransactions,
                            unsigned int iTransactionCount,
                            char **szCsvData,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxInfo **list = pTransactions;
    int iListSize = iTransactionCount;

    char *szCurrRec;
    char *szFinalRec;
    unsigned long ulCsvDataLen = 0;
    tABC_U08Buf buff = ABC_BUF_NULL;

    ABC_CHECK_NULL(pTransactions);

    ABC_BUF_NEW(buff, ABC_CSV_MAX_REC_SZ);
    
    if (iListSize)
    {
        ABC_CHECK_RET(ABC_ExportGenerateHeader(&szCurrRec, pError));
        ABC_BUF_SET_PTR(buff, (unsigned char *) szCurrRec, strlen(szCurrRec));
        ulCsvDataLen += strlen(szCurrRec);
        ABC_BUF_APPEND_PTR(buff, "\n", 1);
        ulCsvDataLen++;
    }
    
    for (int i=0; i < iListSize; i++)
    {
        ABC_CHECK_RET(ABC_ExportGenerateRecord(list[i], &szCurrRec, pError));
        ABC_BUF_APPEND_PTR(buff, szCurrRec, strlen(szCurrRec));
        ulCsvDataLen += strlen(szCurrRec);
        ABC_BUF_APPEND_PTR(buff, "\n", 1);
        ulCsvDataLen++;
    }
    
    /* Allocate enough to hold the entire CSV content and copy from the buffer */
    ABC_ALLOC(szFinalRec, ulCsvDataLen+1);
    strncpy(szFinalRec, (char *) buff.p, (size_t)ulCsvDataLen);
    
    *szCsvData = szFinalRec;
    
    ABC_BUF_CLEAR(buff);
    
exit:
    return cc;
}
