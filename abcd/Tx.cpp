/*
 * Copyright (c) 2014, Airbitz, Inc.
 * All rights reserved.
 *
 * See the LICENSE file for more information.
 */

#include "Tx.hpp"
#include "Context.hpp"
#include "bitcoin/TxDatabase.hpp"
#include "spend/Spend.hpp"
#include "util/Debug.hpp"
#include "util/Util.hpp"
#include "wallet/Details.hpp"
#include "wallet/TxMetadata.hpp"
#include "wallet/Wallet.hpp"
#include <bitcoin/bitcoin.hpp>
#include <ctype.h>
#include <time.h>

namespace abcd {

static Status   txSaveNewTx(Wallet &self, Tx &tx,
                            const std::vector<std::string> &addresses, bool bOutside);
static Status   txGetAmounts(Wallet &self, const std::string &ntxid,
                             int64_t *pAmount, int64_t *pFees);
static Status   txGetOutputs(Wallet &self, const std::string &ntxid,
                             tABC_TxOutput ***paOutputs, unsigned int *pCount);
static int      ABC_TxInfoPtrCompare (const void *a, const void *b);
static void     ABC_TxStrTable(const char *needle, int *table);
static int      ABC_TxStrStr(const char *haystack, const char *needle,
                             tABC_Error *pError);

Status
txSweepSave(Wallet &self,
            const std::string &ntxid, const std::string &txid,
            uint64_t funds)
{
    Tx tx;
    tx.ntxid = ntxid;
    tx.txid = txid;
    tx.timeCreation = time(nullptr);
    tx.internal = true;
    tx.metadata.amountSatoshi = funds;
    tx.metadata.amountFeesAirbitzSatoshi = 0;
    ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                  tx.metadata.amountCurrency, tx.metadata.amountSatoshi,
                  static_cast<Currency>(self.currency())));

    // save the transaction
    ABC_CHECK(self.txs.save(tx));
    self.balanceDirty();

    return Status();
}

Status
txSendSave(Wallet &self,
           const std::string &ntxid, const std::string &txid,
           const std::vector<std::string> &addresses, SendInfo *pInfo)
{
    // set the state
    Tx tx;
    tx.ntxid = ntxid;
    tx.txid = txid;
    tx.timeCreation = time(nullptr);
    tx.internal = true;
    tx.metadata = pInfo->metadata;

    // Add in tx fees to the amount of the tx
    if (self.addresses.has(pInfo->destAddress))
    {
        tx.metadata.amountSatoshi = pInfo->metadata.amountFeesAirbitzSatoshi
                                    + pInfo->metadata.amountFeesMinersSatoshi;
    }
    else
    {
        tx.metadata.amountSatoshi = pInfo->metadata.amountSatoshi
                                    + pInfo->metadata.amountFeesAirbitzSatoshi
                                    + pInfo->metadata.amountFeesMinersSatoshi;
    }

    ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                  tx.metadata.amountCurrency, tx.metadata.amountSatoshi,
                  static_cast<Currency>(self.currency())));

    if (tx.metadata.amountSatoshi > 0)
        tx.metadata.amountSatoshi *= -1;
    if (tx.metadata.amountCurrency > 0)
        tx.metadata.amountCurrency *= -1.0;

    // Save the transaction:
    ABC_CHECK(txSaveNewTx(self, tx, addresses, false));

    if (pInfo->bTransfer)
    {
        Tx receiveTx;
        receiveTx.ntxid = ntxid;
        receiveTx.txid = txid;
        receiveTx.timeCreation = time(nullptr);
        receiveTx.internal = true;
        receiveTx.metadata = pInfo->metadata;

        // Set the payee name:
        receiveTx.metadata.name = self.name();

        //
        // Since this wallet is receiving, it didn't really get charged AB fees
        // This should really be an assert since no transfers should have AB fees
        //
        receiveTx.metadata.amountFeesAirbitzSatoshi = 0;

        ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                      receiveTx.metadata.amountCurrency, receiveTx.metadata.amountSatoshi,
                      static_cast<Currency>(self.currency())));

        if (receiveTx.metadata.amountSatoshi < 0)
            receiveTx.metadata.amountSatoshi *= -1;
        if (receiveTx.metadata.amountCurrency < 0)
            receiveTx.metadata.amountCurrency *= -1.0;

        // save the transaction
        ABC_CHECK(txSaveNewTx(*pInfo->walletDest, receiveTx, addresses, false));
    }

    return Status();
}

Status
txReceiveTransaction(Wallet &self,
                     const std::string &ntxid, const std::string &txid,
                     const std::vector<std::string> &addresses,
                     tABC_BitCoin_Event_Callback fAsyncCallback, void *pData)
{
    Tx temp;

    // Does the transaction already exist?
    if (!self.txs.get(temp, ntxid))
    {
        Tx tx;
        tx.ntxid = ntxid;
        tx.txid = txid;
        tx.timeCreation = time(nullptr);
        tx.internal = false;
        ABC_CHECK(txGetAmounts(self, ntxid,
                               &tx.metadata.amountSatoshi,
                               &tx.metadata.amountFeesMinersSatoshi));
        ABC_CHECK(gContext->exchangeCache.satoshiToCurrency(
                      tx.metadata.amountCurrency, tx.metadata.amountSatoshi,
                      static_cast<Currency>(self.currency())));

        // add the transaction to the address
        ABC_CHECK(txSaveNewTx(self, tx, addresses, true));

        // Update the GUI:
        tABC_AsyncBitCoinInfo info;
        info.pData = pData;
        info.eventType = ABC_AsyncEventType_IncomingBitCoin;
        Status().toError(info.status, ABC_HERE());
        info.szWalletUUID = self.id().c_str();
        info.szTxID = ntxid.c_str();
        info.sweepSatoshi = 0;
        fAsyncCallback(&info);
    }
    else
    {
        ABC_DebugLog("We already have %s", ntxid.c_str());

        // Mark the wallet cache as dirty in case the Tx wasn't included in the current balance
        self.balanceDirty();

        // Update the GUI:
        tABC_AsyncBitCoinInfo info;
        info.pData = pData;
        info.eventType = ABC_AsyncEventType_BalanceUpdate;
        Status().toError(info.status, ABC_HERE());
        info.szWalletUUID = self.id().c_str();
        info.szTxID = ntxid.c_str();
        info.sweepSatoshi = 0;
        fAsyncCallback(&info);
    }

    return Status();
}

/**
 * Saves the a never-before-seen transaction to the sync database,
 * updating the metadata as appropriate.
 *
 * @param bOutside true if this is an outside transaction that needs its
 * details populated from the address database.
 */
Status
txSaveNewTx(Wallet &self, Tx &tx,
            const std::vector<std::string> &addresses, bool bOutside)
{
    // Mark addresses as used:
    TxMetadata metadata;
    for (const auto &i: addresses)
    {
        Address address;
        if (self.addresses.get(address, i))
        {
            // Update the transaction:
            if (address.recyclable)
            {
                address.recyclable = false;
                ABC_CHECK(self.addresses.save(address));
            }
            metadata = address.metadata;
        }
    }

    // Copy the metadata (if any):
    if (bOutside)
    {
        if (tx.metadata.name.empty() && !metadata.name.empty())
            tx.metadata.name = metadata.name;
        if (tx.metadata.notes.empty() && !metadata.notes.empty())
            tx.metadata.notes = metadata.notes;
        if (tx.metadata.category.empty() && !metadata.category.empty())
            tx.metadata.category = metadata.category;
        tx.metadata.bizId = metadata.bizId;
    }
    ABC_CHECK(self.txs.save(tx));
    self.balanceDirty();

    return Status();
}

/**
 * Get the specified transactions.
 * @param ppTransaction     Location to store allocated transaction
 *                          (caller must free)
 */
tABC_CC ABC_TxGetTransaction(Wallet &self,
                             const std::string &ntxid,
                             tABC_TxInfo **ppTransaction,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    Tx tx;
    tABC_TxInfo *pTransaction = structAlloc<tABC_TxInfo>();

    *ppTransaction = NULL;

    // load the transaction
    ABC_CHECK_NEW(self.txs.get(tx, ntxid));

    // steal the data and assign it to our new struct
    pTransaction->szID = stringCopy(tx.ntxid);
    pTransaction->szMalleableTxId = stringCopy(tx.txid);
    pTransaction->timeCreation = tx.timeCreation;
    pTransaction->pDetails = tx.metadata.toDetails();
    ABC_CHECK_NEW(txGetAmounts(self, tx.ntxid,
                               &(pTransaction->pDetails->amountSatoshi),
                               &(pTransaction->pDetails->amountFeesMinersSatoshi)));
    ABC_CHECK_NEW(txGetOutputs(self, tx.ntxid,
                               &pTransaction->aOutputs, &pTransaction->countOutputs));

    // assign final result
    *ppTransaction = pTransaction;
    pTransaction = NULL;

exit:
    ABC_TxFreeTransaction(pTransaction);

    return cc;
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

    NtxidList ntxids = self.txs.list();
    for (const auto &ntxid: ntxids)
    {
        // load it into the info transaction structure
        ABC_CHECK_RET(ABC_TxGetTransaction(self, ntxid, &pTransaction, pError));

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
 * Calculates transaction balances
 */
static Status
txGetAmounts(Wallet &self, const std::string &ntxid,
             int64_t *pAmount, int64_t *pFees)
{
    int64_t totalInSatoshi = 0, totalOutSatoshi = 0;
    int64_t totalMeSatoshi = 0, totalMeInSatoshi = 0;

    bc::hash_digest hash;
    if (!bc::decode_hash(hash, ntxid))
        return ABC_ERROR(ABC_CC_ParseError, "Bad ntxid");
    auto tx = self.txdb.ntxidLookup(hash);

    for (const auto &i: tx.inputs)
    {
        bc::payment_address address;
        bc::extract(address, i.script);

        auto prev = i.previous_output;
        auto tx = self.txdb.txidLookup(prev.hash);
        if (prev.index < tx.outputs.size())
        {
            if (self.addresses.has(address.encoded()))
                totalMeInSatoshi += tx.outputs[prev.index].value;
            totalInSatoshi += tx.outputs[prev.index].value;
        }
    }

    for (const auto &o: tx.outputs)
    {
        bc::payment_address address;
        bc::extract(address, o.script);

        // Do we own this address?
        if (self.addresses.has(address.encoded()))
            totalMeSatoshi += o.value;
        totalOutSatoshi += o.value;
    }

    *pAmount = totalMeSatoshi - totalMeInSatoshi;
    *pFees = totalInSatoshi - totalOutSatoshi;
    return Status();
}

/**
 * Prepares transaction outputs for the advanced details screen.
 */
static Status
txGetOutputs(Wallet &self, const std::string &ntxid,
             tABC_TxOutput ***paOutputs, unsigned int *pCount)
{
    bc::hash_digest hash;
    if (!bc::decode_hash(hash, ntxid))
        return ABC_ERROR(ABC_CC_ParseError, "Bad txid");
    auto tx = self.txdb.ntxidLookup(hash);
    auto txid = bc::encode_hash(bc::hash_transaction(tx));

    // Create the array:
    size_t count = tx.inputs.size() + tx.outputs.size();
    tABC_TxOutput **aOutputs = (tABC_TxOutput **)calloc(count,
                               sizeof(tABC_TxOutput *));
    if (!aOutputs)
        return ABC_ERROR(ABC_CC_NULLPtr, "out of memory");

    // Build output entries:
    int i = 0;
    for (const auto &input: tx.inputs)
    {
        auto prev = input.previous_output;
        bc::payment_address addr;
        bc::extract(addr, input.script);

        tABC_TxOutput *out = (tABC_TxOutput *) malloc(sizeof(tABC_TxOutput));
        out->input = true;
        out->szTxId = stringCopy(bc::encode_hash(prev.hash));
        out->szAddress = stringCopy(addr.encoded());

        auto tx = self.txdb.txidLookup(prev.hash);
        if (prev.index < tx.outputs.size())
        {
            out->value = tx.outputs[prev.index].value;
        }
        aOutputs[i] = out;
        i++;
    }
    for (const auto &output: tx.outputs)
    {
        bc::payment_address addr;
        bc::extract(addr, output.script);

        tABC_TxOutput *out = (tABC_TxOutput *) malloc(sizeof(tABC_TxOutput));
        out->input = false;
        out->value = output.value;
        out->szTxId = stringCopy(txid);
        out->szAddress = stringCopy(addr.encoded());

        aOutputs[i] = out;
        i++;
    }

    *paOutputs = aOutputs;
    *pCount = count;
    return Status();
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
 * Sets the details for a specific existing transaction.
 */
tABC_CC ABC_TxSetTransactionDetails(Wallet &self,
                                    const std::string &ntxid,
                                    const TxMetadata &metadata,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    Tx tx;
    ABC_CHECK_NEW(self.txs.get(tx, ntxid));
    tx.metadata = metadata;
    tx.internal = true;
    ABC_CHECK_NEW(self.txs.save(tx));

exit:
    return cc;
}

/**
 * Gets the details for a specific existing transaction.
 */
tABC_CC ABC_TxGetTransactionDetails(Wallet &self,
                                    const std::string &ntxid,
                                    TxMetadata &result,
                                    tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    Tx tx;
    ABC_CHECK_NEW(self.txs.get(tx, ntxid));
    result = tx.metadata;

exit:
    return cc;
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
                ABC_FREE_STR(pOutput->szTxId);
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
