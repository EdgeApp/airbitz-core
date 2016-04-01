/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "TxInfo.hpp"
#include "TxDetails.hpp"
#include "../abcd/bitcoin/TxCache.hpp"
#include "../abcd/wallet/Wallet.hpp"
#include "../abcd/util/Util.hpp"

namespace abcd {

static void     ABC_TxFreeOutputs(tABC_TxOutput **aOutputs, unsigned int count);
static int      ABC_TxInfoPtrCompare (const void *a, const void *b);
static void     ABC_TxStrTable(const char *needle, int *table);
static int      ABC_TxStrStr(const char *haystack, const char *needle,
                             tABC_Error *pError);

tABC_TxInfo *
makeTxInfo(Wallet &self, const TxInfo &info, const TxStatus &status)
{
    auto out = structAlloc<tABC_TxInfo>();

    // Basic information:
    out->szID = stringCopy(info.txid);
    out->szMalleableTxId = stringCopy(info.txid);

    // Outputs array:
    out->countOutputs = info.ios.size();
    out->aOutputs = arrayAlloc<tABC_TxOutput *>(info.ios.size());
    int i = 0;
    for (const auto &io: info.ios)
    {
        tABC_TxOutput *txo = structAlloc<tABC_TxOutput>();
        txo->input = io.input;
        txo->value = io.value;
        txo->szAddress = stringCopy(io.address);
        out->aOutputs[i++] = txo;
    }

    // Details:
    Tx meta;
    if (self.txs.get(meta, info.ntxid))
    {
        out->pDetails = meta.metadata.toDetails();
        out->timeCreation = meta.timeCreation;
    }
    else
    {
        out->timeCreation = time(nullptr);
        out->pDetails = TxMetadata().toDetails();
    }
    out->pDetails->amountSatoshi = info.balance;
    out->pDetails->amountFeesMinersSatoshi = info.fee;

    // Status:
    out->height = status.height;
    out->bDoubleSpent = status.isDoubleSpent;
    out->bReplaceByFee = status.isReplaceByFee;

    return out;
}

/**
 * Gets the transactions associated with the given wallet.
 *
 * @param startTime         Return transactions after this time
 * @param endTime           Return transactions before this time
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxGetTransactions(Wallet &self,
                              int64_t startTime,
                              int64_t endTime,
                              tABC_TxInfo ***paTransactions,
                              unsigned int *pCount,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    tABC_TxInfo *pTransaction = NULL;
    tABC_TxInfo **aTransactions = NULL;
    unsigned int count = 0;

    const auto infos = self.txCache.list(self.addresses.list());
    for (const auto &info: infos)
    {
        pTransaction = makeTxInfo(self, info.first, info.second);

        if ((endTime == ABC_GET_TX_ALL_TIMES) ||
                (pTransaction->timeCreation >= startTime &&
                 pTransaction->timeCreation < endTime))
        {
            // create space for new entry
            if (aTransactions == NULL)
            {
                ABC_ARRAY_NEW(aTransactions, 1, tABC_TxInfo *);
                count = 1;
            }
            else
            {
                count++;
                ABC_ARRAY_RESIZE(aTransactions, count, tABC_TxInfo *);
            }

            // add it to the array
            aTransactions[count - 1] = pTransaction;
            pTransaction = NULL;
        }
    }

    // if we have more than one, then let's sort them
    if (count > 1)
    {
        // sort the transactions by creation date using qsort
        qsort(aTransactions, count, sizeof(tABC_TxInfo *), ABC_TxInfoPtrCompare);
    }

    // store final results
    *paTransactions = aTransactions;
    aTransactions = NULL;
    *pCount = count;
    count = 0;

exit:
    ABC_TxFreeTransaction(pTransaction);
    ABC_TxFreeTransactions(aTransactions, count);

    return cc;
}

/**
 * Searches transactions associated with the given wallet.
 *
 * @param szQuery           Query to search
 * @param paTransactions    Pointer to store array of transactions info pointers
 * @param pCount            Pointer to store number of transactions
 * @param pError            A pointer to the location to store the error if there is one
 */
tABC_CC ABC_TxSearchTransactions(Wallet &self,
                                 const char *szQuery,
                                 tABC_TxInfo ***paTransactions,
                                 unsigned int *pCount,
                                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_TxInfo **aTransactions = NULL;
    tABC_TxInfo **aSearchTransactions = NULL;
    unsigned int i;
    unsigned int count = 0;
    unsigned int matchCount = 0;

    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    ABC_CHECK_NULL(paTransactions);
    *paTransactions = NULL;
    ABC_CHECK_NULL(pCount);
    *pCount = 0;

    ABC_TxGetTransactions(self, ABC_GET_TX_ALL_TIMES, ABC_GET_TX_ALL_TIMES,
                          &aTransactions, &count, pError);
    ABC_ARRAY_NEW(aSearchTransactions, count, tABC_TxInfo *);
    for (i = 0; i < count; i++)
    {
        tABC_TxInfo *pInfo = aTransactions[i];
        auto satoshi = std::to_string(pInfo->pDetails->amountSatoshi);
        auto currency = std::to_string(pInfo->pDetails->amountCurrency);
        if (ABC_TxStrStr(satoshi.c_str(), szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        }
        if (ABC_TxStrStr(currency.c_str(), szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        }
        if (ABC_TxStrStr(pInfo->pDetails->szName, szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        }
        else if (ABC_TxStrStr(pInfo->pDetails->szCategory, szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        }
        else if (ABC_TxStrStr(pInfo->pDetails->szNotes, szQuery, pError))
        {
            aSearchTransactions[matchCount] = pInfo;
            matchCount++;
            continue;
        }
        else
        {
            ABC_TxFreeTransaction(pInfo);
        }
        aTransactions[i] = NULL;
    }
    if (matchCount > 0)
    {
        ABC_ARRAY_RESIZE(aSearchTransactions, matchCount, tABC_TxInfo *);
    }

    *paTransactions = aSearchTransactions;
    *pCount = matchCount;
    aTransactions = NULL;
exit:
    ABC_FREE(aTransactions);
    return cc;
}

/**
 * Frees the given transaction
 *
 * @param pTransaction Pointer to transaction to free
 */
void ABC_TxFreeTransaction(tABC_TxInfo *pTransaction)
{
    if (pTransaction)
    {
        ABC_FREE_STR(pTransaction->szID);
        ABC_TxFreeOutputs(pTransaction->aOutputs, pTransaction->countOutputs);
        ABC_TxDetailsFree(pTransaction->pDetails);
        ABC_CLEAR_FREE(pTransaction, sizeof(tABC_TxInfo));
    }
}

/**
 * Frees the given array of transactions
 *
 * @param aTransactions Array of transactions
 * @param count         Number of transactions
 */
void ABC_TxFreeTransactions(tABC_TxInfo **aTransactions,
                            unsigned int count)
{
    if (aTransactions && count > 0)
    {
        for (unsigned i = 0; i < count; i++)
        {
            ABC_TxFreeTransaction(aTransactions[i]);
        }

        ABC_FREE(aTransactions);
    }
}

/**
 * This function is used to support sorting an array of tTxInfo pointers via qsort.
 * qsort has the following documentation for the required function:
 *
 * Pointer to a function that compares two elements.
 * This function is called repeatedly by qsort to compare two elements. It shall follow the following prototype:
 *
 * int compar (const void* p1, const void* p2);
 *
 * Taking two pointers as arguments (both converted to const void*). The function defines the order of the elements by returning (in a stable and transitive manner):
 * return value	meaning
 * <0	The element pointed by p1 goes before the element pointed by p2
 * 0	The element pointed by p1 is equivalent to the element pointed by p2
 * >0	The element pointed by p1 goes after the element pointed by p2
 *
 */
static
int ABC_TxInfoPtrCompare (const void *a, const void *b)
{
    tABC_TxInfo **ppInfoA = (tABC_TxInfo **)a;
    tABC_TxInfo *pInfoA = (tABC_TxInfo *)*ppInfoA;
    tABC_TxInfo **ppInfoB = (tABC_TxInfo **)b;
    tABC_TxInfo *pInfoB = (tABC_TxInfo *)*ppInfoB;

    if (pInfoA->timeCreation < pInfoB->timeCreation) return -1;
    if (pInfoA->timeCreation == pInfoB->timeCreation) return 0;
    if (pInfoA->timeCreation > pInfoB->timeCreation) return 1;

    return 0;
}

void ABC_TxFreeOutputs(tABC_TxOutput **aOutputs, unsigned int count)
{
    if ((aOutputs != NULL) && (count > 0))
    {
        for (unsigned i = 0; i < count; i++)
        {
            tABC_TxOutput *pOutput = aOutputs[i];
            if (pOutput)
            {
                ABC_FREE_STR(pOutput->szAddress);
                ABC_CLEAR_FREE(pOutput, sizeof(tABC_TxOutput));
            }
        }
        ABC_CLEAR_FREE(aOutputs, sizeof(tABC_TxOutput *) * count);
    }
}

/**
 * This implemens the KMP failure function. Its the preprocessing before we can
 * search for substrings.
 *
 * @param needle - The string to preprocess
 * @param table - An array of integers the string length of needle
 *
 * Returns 1 if a match is found otherwise returns 0
 *
 */
static
void ABC_TxStrTable(const char *needle, int *table)
{
    size_t pos = 2, cnd = 0;
    table[0] = -1;
    table[1] = 0;
    size_t needle_size = strlen(needle);

    while (pos < needle_size)
    {
        if (tolower(needle[pos - 1]) == tolower(needle[cnd]))
        {
            cnd = cnd + 1;
            table[pos] = cnd;
            pos = pos + 1;
        }
        else if (cnd > 0)
            cnd = table[cnd];
        else
        {
            table[pos] = 0;
            pos = pos + 1;
        }
    }
}

/**
 * This function implemens the KMP string searching algo. This function is
 * used for string matching when searching transactions.
 *
 * @param haystack - The string to search
 * @param needle - The string to find in the haystack
 *
 * Returns 1 if a match is found otherwise returns 0
 *
 */
static
int ABC_TxStrStr(const char *haystack, const char *needle,
                 tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    if (haystack == NULL || needle == NULL)
    {
        return 0;
    }
    int result = -1;
    size_t haystack_size;
    size_t needle_size;
    size_t m = 0, i = 0;
    int *table;

    haystack_size = strlen(haystack);
    needle_size = strlen(needle);

    if (haystack_size == 0 || needle_size == 0)
    {
        return 0;
    }

    ABC_ARRAY_NEW(table, needle_size, int);
    ABC_TxStrTable(needle, table);

    while (m + i < haystack_size)
    {
        if (tolower(needle[i]) == tolower(haystack[m + i]))
        {
            if (i == needle_size - 1)
            {
                result = m;
                break;
            }
            i = i + 1;
        }
        else
        {
            if (table[i] > -1)
            {
                i = table[i];
                m = m + i - table[i];
            }
            else
            {
                i = 0;
                m = m + 1;
            }
        }
    }
exit:
    ABC_FREE(table);
    return result > -1 ? 1 : 0;
}

} // namespace abcd
