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


void debugFillData(tABC_TxInfo *data, char *id)
{
    tABC_TxDetails *pd;
    
    const char *fillString = "XOXO";
    
    const char *test_szID = "abc1234";
    data->szID = calloc(strlen(id) + 1 + strlen(test_szID) + 1, sizeof(char));
    sprintf(data->szID,"%s-%s", id, test_szID);
    
    const char *test_szMID = "MMabc1234";
    data->szMalleableTxId = calloc(strlen(test_szMID)+1, sizeof(char));
    sprintf(data->szMalleableTxId,"%s", test_szMID);
    
    int64_t t1 = 1406813560;
    data->timeCreation = t1;
    
    data->countOutputs = 2;
    
    pd = malloc(sizeof(tABC_TxDetails));
    
    pd->amountSatoshi = 1000000000; /* 10 BTC */
    pd->amountFeesAirbitzSatoshi = 200000000; /* 2 BTC */
    pd->amountFeesMinersSatoshi = 100000000; /* 1 BTC */
    pd->amountCurrency = 9.99;
    
    pd->szName = malloc(strlen(test_szID) + 1);
    strcpy(pd->szName, test_szID);
    
    pd->bizId = 11;
    
    pd->szCategory = malloc(strlen(fillString) + 1);
    strcpy(pd->szCategory, fillString);
    
    char *extraNotes = "What's the Problem\" with this statement?";
    pd->szNotes = malloc(strlen(id) + 1 + strlen(fillString) + strlen(extraNotes) + 1);
    sprintf(pd->szNotes, "%s#%s%s", id, fillString, extraNotes);
    
    pd->attributes = 13;
    
    data->pDetails = pd;
}

tABC_CC debugGetTransactions(tABC_TxInfo ***ptransactions, int *pnumoftxs, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    
    
    int ptrSize = 5;
    char idstr[16];
    tABC_TxInfo **data;
    char *debugRec;
    
    *pnumoftxs = ptrSize;
    
    ABC_ALLOC(data, (sizeof(tABC_TxInfo *) * ptrSize));
    
    for (int x=0; x < ptrSize; x++)
    {
        ABC_ALLOC(data[x], sizeof(tABC_TxInfo));
    }
    
    ABC_ALLOC(debugRec, ABC_CSV_MAX_REC_SZ+1);
    
    for (int i=0; i < ptrSize; i++)
    {
        sprintf(idstr,"I%d",i);
        printf("ID=%s\t",idstr);
        debugFillData(data[i], idstr);
        printf("id=%s\n", (*data)[i].szID);
    }
    
    *ptransactions = data;
    
exit:
    return cc;
    
}

tABC_CC ABC_ExportGenerateHeader(char **szCsvRec, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    char **out = szCsvRec;

    /* header */
    char *szCsvId = "ID";
    char *szCsvMaleableId = "ID2";
    char *szTimeCreation = "CREATE_TIME";
    
    /* details */
    char *szAmtSatoshi = "AMT_SATOSHI";
    char *szAmtAirbitzSatoshi = "AMT_SATOSHI_FEES_AB";
    char *szAmtFeesMinersSatoshi = "AMT_SATOSHI_FEES_MINERS";
    char *szCurrency = "CURRENCY";
    char *szName = "PAYEE_PAYER_NAME"; /* payee or payer */
    char *szBizId = "BUSINESS_ID";
    char *szCategory = "CATEGORY";
    char *szNotes = "NOTE";

    ABC_CHECK_NULL(szCsvRec);

    /* Allocated enough space for the header descriptions */
    ABC_ALLOC(*szCsvRec, ABC_CSV_MAX_REC_SZ * 2);
    
    /* build the entire record */
    sprintf(*out,
            "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s",
            szCsvId, szCsvMaleableId, szTimeCreation,
            szAmtSatoshi, szAmtAirbitzSatoshi, szAmtFeesMinersSatoshi, szCurrency,
            szName, szBizId, szCategory, szNotes,
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
    char *szCsvId;
    char *szCsvMaleableId;
    char *szTimeCreation;
    
    /* details */
    char *szAmtSatoshi;
    char *szAmtAirbitzSatoshi;
    char *szAmtFeesMinersSatoshi;
    char *szCurrency;
    char *szName; /* payee or payer */
    char *szBizId;
    char *szCategory;
    char *szNotes;

    tABC_CSV tmpCsvVar;

    ABC_CHECK_NULL(data);

    ABC_CSV_INIT(tmpCsvVar, data->szID);
    ABC_CSV_FMT(tmpCsvVar, szCsvId);

    ABC_CSV_INIT(tmpCsvVar, data->szMalleableTxId);
    ABC_CSV_FMT(tmpCsvVar, szCsvMaleableId);

    ABC_CSV_INIT2(tmpCsvVar, pData->timeCreation, PRIu64);
    ABC_CSV_FMT(tmpCsvVar, szTimeCreation);

    ABC_CSV_INIT2(tmpCsvVar, pData->pDetails->amountSatoshi, PRIu64);
    ABC_CSV_FMT(tmpCsvVar, szAmtSatoshi);

    ABC_CSV_INIT2(tmpCsvVar, pData->pDetails->amountFeesAirbitzSatoshi, PRIu64);
    ABC_CSV_FMT(tmpCsvVar, szAmtAirbitzSatoshi);

    ABC_CSV_INIT2(tmpCsvVar, pData->pDetails->amountFeesMinersSatoshi, PRIu64);
    ABC_CSV_FMT(tmpCsvVar, szAmtFeesMinersSatoshi);

    ABC_CSV_INIT2(tmpCsvVar, pData->pDetails->amountCurrency, "0.8f");
    ABC_CSV_FMT(tmpCsvVar, szCurrency);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szName);
    ABC_CSV_FMT(tmpCsvVar, szName);

    ABC_CSV_INIT2(tmpCsvVar, pData->pDetails->bizId, "d");
    ABC_CSV_FMT(tmpCsvVar, szBizId);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szCategory);
    ABC_CSV_FMT(tmpCsvVar, szCategory);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szNotes);
    ABC_CSV_FMT(tmpCsvVar, szNotes);

    /* Allocated enough space for one CSV REC - Determined by adding all field when quoted */
    ABC_ALLOC(*szCsvRec, ABC_CSV_MAX_REC_SZ+1);
    
    /* build the entire record */
    sprintf(*out,
            "%s%s%s%s%s%s%s%s%s%s%s%s",
            szCsvId, szCsvMaleableId, szTimeCreation,
            szAmtSatoshi, szAmtAirbitzSatoshi, szAmtFeesMinersSatoshi, szCurrency,
            szName, szBizId, szCategory, szNotes,
            ABC_CSV_REC_TERM_VALUE);

    ABC_FREE(szCsvId);
    ABC_FREE(szCsvMaleableId);
    ABC_FREE(szTimeCreation);
    ABC_FREE(szAmtSatoshi);
    ABC_FREE(szAmtAirbitzSatoshi);
    ABC_FREE(szAmtFeesMinersSatoshi);
    ABC_FREE(szCurrency);
    ABC_FREE(szName);
    ABC_FREE(szBizId);
    ABC_FREE(szCategory);
    ABC_FREE(szNotes);

    
exit:
    return cc;
}

tABC_CC ABC_ExportFormatCsv(tABC_TxInfo **pTransactions,
                            int iTransactionCount,
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
