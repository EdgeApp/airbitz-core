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


#include "ABC_Export.h"
#include "csv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>

#define MAX_DATE_TIME_SIZE 20
#define MAX_AMOUNT_STRING_SIZE 20 // 21 million + 8 decimals + padding. ~20 digits

/* Maxium size of CSV record and fields in characters */
#define ABC_CSV_MAX_REC_SZ  16384
#define ABC_CSV_MAX_FLD_SZ  4096

#define ABC_CSV_DFLT_DELIMITER "|"
#define ABC_CSV_ALT1_DELIMITER ","

#define ABC_CSV_REC_TERM_NAME "VER"
#define ABC_CSV_REC_TERM_VALUE "1"


#define ABC_CSV(type)    struct { \
                                      type ov[ABC_CSV_MAX_FLD_SZ]; \
                                      type qv[ABC_CSV_MAX_FLD_SZ]; \
                                      size_t os; \
                                      size_t qs; \
                                }

#define ABC_CSV_INIT(fld, src)  { \
                                  strncpy((fld).ov, src, ABC_CSV_MAX_FLD_SZ); \
                                  memset((fld).qv, 0, sizeof((fld).qv)); \
                                  (fld).os = strlen(src); \
                                  (fld).qs = 0; \
                                }

#define ABC_CSV_INIT2(fld, src, type) {\
                                        char _tmpXform[ABC_CSV_MAX_FLD_SZ]; \
                                        snprintf(_tmpXform,ABC_CSV_MAX_FLD_SZ,"%" type, src); \
                                        ABC_CSV_INIT(fld, _tmpXform); \
                                      }

#define ABC_CSV_FMT(v, d)       { \
                                  size_t _sz = 0; \
                                  _sz = csv_write(v.qv, sizeof(v.qv), v.ov, v.os); \
                                  v.qs = _sz; \
                                  ABC_STR_NEW(d, v.qs+1+1); \
                                  snprintf(d, v.qs+1+1,"%s%s", v.qv, ABC_CSV_ALT1_DELIMITER); \
                                }


typedef ABC_CSV(char) tABC_CSV;

tABC_CC ABC_ExportGenerateHeader(char **szCsvRec, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char **out = szCsvRec;

    /* header */
    const char *szDateCreation = "DATE";
    const char *szTimeCreation = "TIME";
    const char *szName = "PAYEE_PAYER_NAME"; /* payee or payer */
    const char *szAmtBTC = "AMT_BTC";
    const char *szCurrency = "USD";
    const char *szCategory = "CATEGORY";
    const char *szNotes = "NOTES";
    const char *szAmtAirbitzBTC = "AMT_BTC_FEES_AB";
    const char *szAmtFeesMinersBTC = "AMT_BTC_FEES_MINERS";
    const char *szInputAddresses = "IN_ADDRESSES";
    const char *szOutputAddresses = "OUT_ADDRESSES";
    const char *szCsvTxid = "TXID";
    const char *szCsvNtxid = "NTXID";

    ABC_CHECK_NULL(szCsvRec);

    /* Allocated enough space for the header descriptions */
    ABC_STR_NEW(*szCsvRec, ABC_CSV_MAX_REC_SZ);

    /* build the entire record */
    snprintf(*out, ABC_CSV_MAX_FLD_SZ,
             "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
             szDateCreation,
             szTimeCreation,
             szName,
             szAmtBTC,
             szCurrency,
             szCategory,
             szNotes,
             szAmtAirbitzBTC,
             szAmtFeesMinersBTC,
             szInputAddresses,
             szOutputAddresses,
             szCsvTxid,
             szCsvNtxid,
             ABC_CSV_REC_TERM_NAME);
exit:
    return cc;
}

tABC_CC ABC_ExportGetAddresses(tABC_TxInfo *pData,
                               char **szAddresses,
                               bool bInputs,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    unsigned i;
    int numAddr = 0;
    char *szAmount = NULL;

    // Clear string if allocated
    if (*szAddresses != NULL)
    {
        strcpy(*szAddresses, "");
    }

    for (i = 0; i < pData->countOutputs; i++)
    {
        bool doCopy = false;
        if (pData->aOutputs[i]->input)
        {
            if (bInputs)
            {
                // Found an input and want only inputs
                doCopy = true;
            }
        }
        else if (!bInputs)
        {
            // Found an output and want only outputs
            doCopy = true;
        }

        if (doCopy)
        {
            int lengthNeeded;

            ABC_STR_NEW(szAmount, MAX_AMOUNT_STRING_SIZE)

            ABC_CHECK_RET(ABC_FormatAmount(pData->aOutputs[i]->value, &szAmount, ABC_BITCOIN_DECIMAL_PLACES, false, pError));

            lengthNeeded = ABC_STRLEN(*szAddresses) + ABC_STRLEN(pData->aOutputs[i]->szAddress) + ABC_STRLEN(szAmount) + 3;

            if (lengthNeeded > ABC_CSV_MAX_FLD_SZ) break;

            ABC_ARRAY_RESIZE(*szAddresses, lengthNeeded, char);

            if (numAddr == 0)
            {
                snprintf(*szAddresses, lengthNeeded, "%s:%s", pData->aOutputs[i]->szAddress, szAmount);
            }
            else
            {
                snprintf(*szAddresses, lengthNeeded, "%s %s:%s", *szAddresses, pData->aOutputs[i]->szAddress, szAmount);
            }
            numAddr++;
        }
    }

exit:
    ABC_FREE(szAmount);
    return cc;
}

tABC_CC ABC_ExportGenerateRecord(tABC_TxInfo *data, char **szCsvRec, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxInfo *pData = data;
    char **out = szCsvRec;

    /* header */
    char *szDateCreation;
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
    char *szCsvTxid;
    char *szCsvNtxid;

    char buff[MAX_DATE_TIME_SIZE];
    char *pFormatted = NULL;

    time_t t = (time_t) pData->timeCreation;
    struct tm *tmptr = localtime(&t);

    tABC_CSV tmpCsvVar;

    ABC_CHECK_NULL(data);

    if (!strftime(buff, sizeof buff, "%Y-%m-%d", tmptr))
    {
        cc = ABC_CC_Error;
        goto exit;
    }
    ABC_CSV_INIT(tmpCsvVar, buff);
    ABC_CSV_FMT(tmpCsvVar, szDateCreation);

    if (!strftime(buff, sizeof buff, "%H:%M", tmptr))
    {
        cc = ABC_CC_Error;
        goto exit;
    }
    ABC_CSV_INIT(tmpCsvVar, buff);
    ABC_CSV_FMT(tmpCsvVar, szTimeCreation);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szName);
    ABC_CSV_FMT(tmpCsvVar, szName);

    ABC_CHECK_RET(ABC_FormatAmount(pData->pDetails->amountSatoshi,&pFormatted, ABC_BITCOIN_DECIMAL_PLACES, true, pError));
    ABC_CSV_INIT(tmpCsvVar, pFormatted);
    ABC_CSV_FMT(tmpCsvVar, szAmtBTC);

    ABC_CSV_INIT2(tmpCsvVar, pData->pDetails->amountCurrency, "0.2f");
    ABC_CSV_FMT(tmpCsvVar, szCurrency);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szCategory);
    ABC_CSV_FMT(tmpCsvVar, szCategory);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szNotes);
    ABC_CSV_FMT(tmpCsvVar, szNotes);

    ABC_CHECK_RET(ABC_FormatAmount(pData->pDetails->amountFeesAirbitzSatoshi,&pFormatted, ABC_BITCOIN_DECIMAL_PLACES, true, pError));
    ABC_CSV_INIT(tmpCsvVar, pFormatted);
    ABC_CSV_FMT(tmpCsvVar, szAmtAirbitzBTC);

    ABC_CHECK_RET(ABC_FormatAmount(pData->pDetails->amountFeesMinersSatoshi,&pFormatted, ABC_BITCOIN_DECIMAL_PLACES, true, pError));
    ABC_CSV_INIT(tmpCsvVar, pFormatted);
    ABC_CSV_FMT(tmpCsvVar, szAmtFeesMinersBTC);

    ABC_CHECK_RET(ABC_ExportGetAddresses(pData, &pFormatted, true, pError));
    ABC_CSV_INIT(tmpCsvVar, pFormatted);
    ABC_CSV_FMT(tmpCsvVar, szInputAddresses);

    ABC_CHECK_RET(ABC_ExportGetAddresses(pData, &pFormatted, false, pError));
    ABC_CSV_INIT(tmpCsvVar, pFormatted);
    ABC_CSV_FMT(tmpCsvVar, szOutputAddresses);

    ABC_CSV_INIT(tmpCsvVar, data->szMalleableTxId);
    ABC_CSV_FMT(tmpCsvVar, szCsvTxid);

    ABC_CSV_INIT(tmpCsvVar, data->szID);
    ABC_CSV_FMT(tmpCsvVar, szCsvNtxid);

    /* Allocated enough space for one CSV REC - Determined by adding all field when quoted */
    ABC_STR_NEW(*szCsvRec, ABC_CSV_MAX_REC_SZ+1);

    /* build the entire record */
    snprintf(*out, ABC_CSV_MAX_FLD_SZ,
             "%s%s%s%s%s%s%s%s%s%s%s%s%s%s",
             szDateCreation,
             szTimeCreation,
             szName,
             szAmtBTC,
             szCurrency,
             szCategory,
             szNotes,
             szAmtAirbitzBTC,
             szAmtFeesMinersBTC,
             szInputAddresses,
             szOutputAddresses,
             szCsvTxid,
             szCsvNtxid,
             ABC_CSV_REC_TERM_VALUE);

    ABC_FREE(szTimeCreation);
    ABC_FREE(szDateCreation);
    ABC_FREE(szName);
    ABC_FREE(szAmtBTC);
    ABC_FREE(szCurrency);
    ABC_FREE(szCategory);
    ABC_FREE(szNotes);
    ABC_FREE(szAmtAirbitzBTC);
    ABC_FREE(szAmtFeesMinersBTC);
    ABC_FREE(szCsvTxid);
    ABC_FREE(szCsvNtxid);
    ABC_FREE(pFormatted);


exit:
    return cc;
}

tABC_CC ABC_ExportFormatCsv(tABC_TxInfo **pTransactions,
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
    ABC_STR_NEW(szFinalRec, ulCsvDataLen+1);
    strncpy(szFinalRec, (char *) buff.p, (size_t)ulCsvDataLen);

    *szCsvData = szFinalRec;

    ABC_BUF_CLEAR(buff);

exit:
    return cc;
}
