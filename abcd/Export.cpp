/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Export.hpp"
#include "util/U08Buf.hpp"
#include "util/Util.hpp"
#include "csv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <string>
#include <math.h>

namespace abcd {

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
             "%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s\n",
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

            ABC_CHECK_RET(ABC_FormatAmount(pData->aOutputs[i]->value, &szAmount,
                                           ABC_BITCOIN_DECIMAL_PLACES, false, pError));

            lengthNeeded = ABC_STRLEN(*szAddresses) + ABC_STRLEN(
                               pData->aOutputs[i]->szAddress) + ABC_STRLEN(szAmount) + 3;

            if (lengthNeeded > ABC_CSV_MAX_FLD_SZ) break;

            ABC_ARRAY_RESIZE(*szAddresses, lengthNeeded, char);

            if (numAddr == 0)
            {
                snprintf(*szAddresses, lengthNeeded, "%s:%s", pData->aOutputs[i]->szAddress,
                         szAmount);
            }
            else
            {
                snprintf(*szAddresses, lengthNeeded, "%s %s:%s", *szAddresses,
                         pData->aOutputs[i]->szAddress, szAmount);
            }
            numAddr++;
        }
    }

exit:
    ABC_FREE(szAmount);
    return cc;
}

tABC_CC ABC_ExportGenerateRecord(tABC_TxInfo *data, char **szCsvRec,
                                 tABC_Error *pError)
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

    ABC_CHECK_RET(ABC_FormatAmount(pData->pDetails->amountSatoshi,&pFormatted,
                                   ABC_BITCOIN_DECIMAL_PLACES, true, pError));
    ABC_CSV_INIT(tmpCsvVar, pFormatted);
    ABC_CSV_FMT(tmpCsvVar, szAmtBTC);

    ABC_CSV_INIT2(tmpCsvVar, pData->pDetails->amountCurrency, "0.2f");
    ABC_CSV_FMT(tmpCsvVar, szCurrency);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szCategory);
    ABC_CSV_FMT(tmpCsvVar, szCategory);

    ABC_CSV_INIT(tmpCsvVar, pData->pDetails->szNotes);
    ABC_CSV_FMT(tmpCsvVar, szNotes);

    ABC_CHECK_RET(ABC_FormatAmount(pData->pDetails->amountFeesAirbitzSatoshi,
                                   &pFormatted, ABC_BITCOIN_DECIMAL_PLACES, true, pError));
    ABC_CSV_INIT(tmpCsvVar, pFormatted);
    ABC_CSV_FMT(tmpCsvVar, szAmtAirbitzBTC);

    ABC_CHECK_RET(ABC_FormatAmount(pData->pDetails->amountFeesMinersSatoshi,
                                   &pFormatted, ABC_BITCOIN_DECIMAL_PLACES, true, pError));
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
             "%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
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

    std::string out;
    {
        AutoString szCurrRec;
        ABC_CHECK_RET(ABC_ExportGenerateHeader(&szCurrRec.get(), pError));
        out += szCurrRec;
    }

    for (unsigned i=0; i < iTransactionCount; i++)
    {
        AutoString szCurrRec;
        ABC_CHECK_RET(ABC_ExportGenerateRecord(pTransactions[i], &szCurrRec.get(),
                                               pError));
        out += szCurrRec;
    }

    *szCsvData = stringCopy(out);

exit:
    return cc;
}

tABC_CC ABC_ExportQBOGenerateHeader(std::string *header, std::string date_today, tABC_Error *pError)
{

    *header = "OFXHEADER:100\n"
                     "DATA:OFXSGML\n"
                     "VERSION:102\n"
                     "SECURITY:NONE\n"
                     "ENCODING:USASCII\n"
                     "CHARSET:1252\n"
                     "COMPRESSION:NONE\n"
                     "OLDFILEUID:NONE\n"
                     "NEWFILEUID:NONE\n\n"
                     "<OFX>\n"
                     "<SIGNONMSGSRSV1>\n"
                     "<SONRS>\n"
                     "<STATUS>\n"
                     "<CODE>0\n"
                     "<SEVERITY>INFO\n"
                     "</STATUS>\n"
                     "<DTSERVER>" + date_today + "\n"
                     "<LANGUAGE>ENG\n"
                     "<INTU.BID>3000\n"
                     "</SONRS>\n"
                     "</SIGNONMSGSRSV1>\n"
                     "<BANKMSGSRSV1>\n"
                     "<STMTTRNRS>\n"
                     "<TRNUID>" + date_today + "\n"
                     "<STATUS>\n"
                     "<CODE>0\n"
                     "<SEVERITY>INFO\n"
                     "<MESSAGE>OK\n"
                     "</STATUS>\n"
                     "<STMTRS>\n"
                     "<CURDEF>USD\n"
                     "<BANKACCTFROM>\n"
                     "<BANKID>999999999\n"
                     "<ACCTID>999999999999\n"
                     "<ACCTTYPE>CHECKING\n"
                     "</BANKACCTFROM>\n\n"
                     "<BANKTRANLIST>\n"
                     "<DTSTART>" + date_today + "\n"
                     "<DTEND>" + date_today + "\n";


    tABC_CC cc = ABC_CC_Ok;
    return cc;
}

#define MAX_MEMO_SIZE 253

tABC_CC ABC_ExportQBOGenerateRecord(tABC_TxInfo *data, std::string *transactions,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxDetails *pDetails = data->pDetails;

    char   *amountFormatted = NULL;
    ABC_FormatAmount(pDetails->amountSatoshi,&amountFormatted,
                     ABC_BITCOIN_DECIMAL_PLACES - (ABC_DENOMINATION_UBTC * 3), true, pError);
    if (pError->code)
    {
        return pError->code;
    }

    char buff[MAX_DATE_TIME_SIZE];
    char buffMemo[MAX_MEMO_SIZE];
    char buffExRate[10];
    std::string transaction;
    std::string trtype;
    std::string date_time;
    std::string amount = amountFormatted;
    std::string txid = data->szMalleableTxId;
    std::string payee = pDetails->szName;
    std::string trname;
    std::string memo;
    std::string exchangeRate;

    // Transaction type
    if (pDetails->amountSatoshi > 0)
        trtype = "CREDIT";
    else
        trtype = "DEBIT";

    // Transaction date/time
    time_t t = (time_t) data->timeCreation;
    struct tm *tmptr = localtime(&t);

    if (!strftime(buff, sizeof buff, "%Y%m%d%H%M%S.000", tmptr))
    {
        cc = ABC_CC_Error;
        return cc;
    }
    date_time = buff;

    // Payee name
    if (payee.length() > 0)
        trname = "  <NAME>" + payee + "\n";
    else
        trname = "";

    // Transaction amount
    double fAmount = ((double) pDetails->amountSatoshi) / 100; // Convert to bits

    // Exchange rate
    double fExchangeRate = pDetails->amountCurrency / fAmount;
    fExchangeRate = fabs(fExchangeRate);
    int s = std::snprintf(buffExRate, sizeof(buffExRate),
                          "%.6f", fExchangeRate);
    exchangeRate = buffExRate;

    // Memo
    s = std::snprintf(buffMemo, sizeof(buffMemo),
                      "// Rate=%s USD=%.2f category=\"%s\" memo=\"%s\"",
                      exchangeRate.c_str(), fabs(pDetails->amountCurrency), pDetails->szCategory, pDetails->szNotes);
    memo = buffMemo;

    transaction = "<STMTTRN>\n"
            "  <TRNTYPE>" + trtype + "\n"
            "  <DTPOSTED>" + date_time + "\n"
            "  <TRNAMT>" + amount + "\n"
            "  <FITID>" + txid + "\n"
            + trname +
            "  <MEMO>" + memo + "\n"
            "  <CURRENCY>" + "\n"
            "    <CURRATE>" + exchangeRate + "\n"
            "    <CURSYM>USD" + "\n"
            "  </CURRENCY>" + "\n"
            "</STMTTRN>\n";

    ABC_FREE(amountFormatted)

    (*transactions).assign(transaction);

    return cc;
}


tABC_CC ABC_ExportFormatQBO(tABC_TxInfo **pTransactions,
                            unsigned int iTransactionCount,
                            char **szQBOData,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    std::time_t rawtime;
    std::tm* timeinfo;
    char buffer [80];

    std::time(&rawtime);
    timeinfo = std::localtime(&rawtime);

    std::strftime(buffer,80,"%Y%m%d%H%M%S.000",timeinfo);

    std::string date_today = buffer;

    std::string out;
    {
        std::string header;
        ABC_CHECK_RET(ABC_ExportQBOGenerateHeader(&header, date_today, pError));
        out += header;
    }

    for (unsigned i=0; i < iTransactionCount; i++)
    {
        std::string transactions;
        ABC_CHECK_RET(ABC_ExportQBOGenerateRecord(pTransactions[i], &transactions,
                                               pError));
        out += transactions;
    }

    // Write footer
    out += "</BANKTRANLIST>\n"
                   "<LEDGERBAL>\n"
                   "<BALAMT>0.00\n"
                   "<DTASOF>" + date_today + "\n"
                   "</LEDGERBAL>\n"
                   "<AVAILBAL>\n"
                   "<BALAMT>0.00\n"
                   "<DTASOF>" +  date_today + "\n"
                   "</AVAILBAL>\n"
                   "</STMTRS>\n"
                   "</STMTTRNRS>\n"
                   "</BANKMSGSRSV1>\n"
                   "</OFX>\n";

    *szQBOData = (char *) malloc(sizeof(char) * (out.length() + 1));
    strcpy(*szQBOData, out.c_str());

    exit:
    return cc;
}



} // namespace abcd
