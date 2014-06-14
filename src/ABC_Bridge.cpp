/**
 * @file
 * AirBitz C++ wrappers.
 *
 * This file contains a bridge between the plain C code in the core, and
 * the C++ code in libbitcoin and friends.
 *
 * @author William Swanson
 * @version 0.1
 */

#include "ABC_Bridge.h"
#include "ABC_URL.h"
#include <curl/curl.h>
#include <wallet/uri.hpp>
#include <wallet/hd_keys.hpp>
#include <wallet/key_formats.hpp>
#include <unordered_map>
#include <bitcoin/bitcoin.hpp>
#include <bitcoin/utility/base58.hpp>
#include <bitcoin/utility/sha256.hpp>
#include <bitcoin/client.hpp>

#define FALLBACK_OBELISK "tcp://obelisk1.airbitz.co:9091"

#define AB_MIN(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a < _b ? _a : _b; })
#define AB_MAX(a,b) \
   ({ __typeof__ (a) _a = (a); \
       __typeof__ (b) _b = (b); \
     _a > _b ? _a : _b; })

struct WatcherInfo
{
    libwallet::watcher *watcher;
    libwallet::watcher::callback callback;
    std::set<std::string> addresses;
    char *szWalletUUID;
    char *szUserName;
    char *szPassword;
};

typedef std::string WalletUUID;
static std::map<WalletUUID, WatcherInfo*> watchers_;

static tABC_CC     ABC_BridgeTxErrorHandler(libwallet::unsigned_transaction_type *utx, tABC_Error *pError);
static void        ABC_BridgeTxCallback(WatcherInfo *watcherInfo, const libbitcoin::transaction_type& tx);
static void        ABC_BridgeAppendOutput(bc::transaction_output_list& outputs, uint64_t amount, const bc::short_hash &addr);
static tABC_CC     ABC_BridgeStringToEc(char *privKey, bc::elliptic_curve_key& key, tABC_Error *pError);
static void        ABC_BridgeVectorCopy(char ***arr, std::vector<std::string> addresses);
static uint64_t    ABC_BridgeCalcAbFees(uint64_t amount, tABC_AccountGeneralInfo *pInfo);
static uint64_t    ABC_BridgeCalcMinerFees(libwallet::unsigned_transaction_type *utx, tABC_AccountGeneralInfo *pInfo);
static std::string ABC_BridgeWatcherFile(const char *szUserName, const char *szPassword, const char *szWalletUUID);
static tABC_CC     ABC_BridgeWatcherLoad(WatcherInfo *watcherInfo, tABC_Error *pError);
static void        ABC_BridgeWatcherSerializeAsync(WatcherInfo *watcherInfo);
static void        *ABC_BridgeWatcherSerialize(void *pData);
static tABC_CC     ABC_BridgeBlockhainPostTx(libwallet::unsigned_transaction_type *utx, tABC_Error *pError);
static size_t      ABC_BridgeCurlWriteData(void *pBuffer, size_t memberSize, size_t numMembers, void *pUserData);
static std::string ABC_BridgeNonMalleableTxId(const bc::transaction_type& tx);

/**
 * Parses a Bitcoin URI and creates an info struct with the data found in the URI.
 *
 * @param szURI     URI to parse
 * @param ppInfo    Pointer to location to store allocated info struct.
 *                  If a member is not present in the URI, the corresponding
 *                  string poiner in the struct will be NULL.
 * @param pError    A pointer to the location to store the error if there is one
 */
tABC_CC ABC_BridgeParseBitcoinURI(const char *szURI,
                            tABC_BitcoinURIInfo **ppInfo,
                            tABC_Error *pError)
{
    libwallet::uri_parse_result result;
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_BitcoinURIInfo *pInfo = NULL;

    ABC_CHECK_NULL(szURI);
    ABC_CHECK_ASSERT(strlen(szURI) > 0, ABC_CC_Error, "No URI provided");
    ABC_CHECK_NULL(ppInfo);
    *ppInfo = NULL;

    // allocate initial struct
    ABC_ALLOC_ARRAY(pInfo, 1, tABC_BitcoinURIInfo);

    // parse and extract contents
    if (!libwallet::uri_parse(szURI, result))
        ABC_RET_ERROR(ABC_CC_ParseError, "Malformed bitcoin URI");
    if (result.address)
        ABC_STRDUP(pInfo->szAddress, result.address.get().encoded().c_str());
    if (result.amount)
        pInfo->amountSatoshi = result.amount.get();
    if (result.label)
        ABC_STRDUP(pInfo->szLabel, result.label.get().c_str());
    if (result.message)
        ABC_STRDUP(pInfo->szMessage, result.message.get().c_str());

    // assign created info struct
    *ppInfo = pInfo;
    pInfo = NULL; // do this so we don't free below what we just gave the caller

exit:
    ABC_BridgeFreeURIInfo(pInfo);

    return cc;
}

/**
 * Frees the memory allocated by ABC_BridgeParseBitcoinURI.
 *
 * @param pInfo Pointer to allocated info struct.
 */
void ABC_BridgeFreeURIInfo(tABC_BitcoinURIInfo *pInfo)
{
    if (pInfo != NULL)
    {
        ABC_FREE_STR(pInfo->szLabel);

        ABC_FREE_STR(pInfo->szAddress);

        ABC_FREE_STR(pInfo->szMessage);

        ABC_CLEAR_FREE(pInfo, sizeof(tABC_BitcoinURIInfo));
    }
}

/**
 * Parses a Bitcoin amount string to an integer.
 * @param the amount to parse, in bitcoins
 * @param the integer value, in satoshis, or ABC_INVALID_AMOUNT
 * if something goes wrong.
 * @param decimal_places set to ABC_BITCOIN_DECIMAL_PLACE to convert
 * bitcoin to satoshis.
 */
tABC_CC ABC_BridgeParseAmount(const char *szAmount,
                              int64_t *pAmountOut,
                              unsigned decimalPlaces)
{
    *pAmountOut = libwallet::parse_amount(szAmount, decimalPlaces);
    return ABC_CC_Ok;
}

/**
 * Formats a Bitcoin integer amount as a string, avoiding the rounding
 * problems typical with floating-point math.
 * @param amount the number of satoshis
 * @param pszAmountOut a pointer that will hold the output string. The
 * caller frees the returned value.
 * @param decimal_places set to ABC_BITCOIN_DECIMAL_PLACE to convert
 * satoshis to bitcoins.
 */
tABC_CC ABC_BridgeFormatAmount(int64_t amount,
                               char **pszAmountOut,
                               unsigned decimalPlaces,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::string out;

    ABC_CHECK_NULL(pszAmountOut);

    out = libwallet::format_amount(amount, decimalPlaces);
    ABC_STRDUP(*pszAmountOut, out.c_str());

exit:
    return cc;
}

/**
 *
 */
tABC_CC ABC_BridgeEncodeBitcoinURI(char **pszURI,
                                   tABC_BitcoinURIInfo *pInfo,
                                   tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    libwallet::uri_writer writer;
    if (pInfo->szAddress)
        writer.write_address(pInfo->szAddress);
    if (pInfo->amountSatoshi)
        writer.write_amount(pInfo->amountSatoshi);
    if (pInfo->szLabel)
        writer.write_param("label", pInfo->szLabel);
    if (pInfo->szMessage)
        writer.write_param("message", pInfo->szMessage);

    ABC_STRDUP(*pszURI, writer.string().c_str());

exit:
    return cc;
}

/**
 * Converts a block of data to a Base58-encoded string.
 *
 * @param Data Buffer of data to convert.
 * @param pszBase58 Output string, allocated by this function.
 */
tABC_CC ABC_BridgeBase58Encode(tABC_U08Buf Data, char **pszBase58, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    libbitcoin::data_chunk in(Data.p, Data.end);
    std::string out = libbitcoin::encode_base58(in);

    ABC_STRDUP(*pszBase58, out.c_str());

exit:
    return cc;
}

/**
 * Converts a Base58-encoded string to a block of data.
 *
 * @param szBase58 The string to convert.
 * @param pData A buffer structure that will be pointed at the newly-allocated output data.
 */
tABC_CC ABC_BridgeBase58Decode(const char *szBase58, tABC_U08Buf *pData, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    libbitcoin::data_chunk out;

    std::string in = szBase58;
    if (!libbitcoin::is_base58(in))
        ABC_RET_ERROR(ABC_CC_ParseError, "Not Base58 data");

    out = libbitcoin::decode_base58(in);

    pData->p = static_cast<unsigned char*>(malloc(out.size()));
    ABC_CHECK_ASSERT(pData->p != NULL, ABC_CC_NULLPtr, "malloc failed (returned NULL)");
    pData->end = pData->p + out.size();
    memcpy(pData->p, out.data(), out.size());

exit:
    return cc;
}

/**
 * Calculates a public address for the HD wallet main external chain.
 * @param pszPubAddress set to the newly-generated address, or set to NULL if
 * there is a math error. If that happens, add 1 to N and try again.
 * @param PrivateSeed any amount of random data to seed the generator
 * @param N the index of the key to generate
 */
tABC_CC ABC_BridgeGetBitcoinPubAddress(char **pszPubAddress,
                                       tABC_U08Buf PrivateSeed,
                                       int32_t N,
                                       tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    libbitcoin::data_chunk seed(PrivateSeed.p, PrivateSeed.end);
    libwallet::hd_private_key m(seed);
    libwallet::hd_private_key m0 = m.generate_private_key(0);
    libwallet::hd_private_key m00 = m0.generate_private_key(0);
    libwallet::hd_private_key m00n = m00.generate_private_key(N);
    if (m00n.valid())
    {
        std::string out = m00n.address().encoded();
        ABC_STRDUP(*pszPubAddress, out.c_str());
    }
    else
    {
        *pszPubAddress = nullptr;
    }

exit:
    return cc;
}

tABC_CC ABC_BridgeGetBitcoinPrivAddress(char **pszPrivAddress,
                                        tABC_U08Buf PrivateSeed,
                                        int32_t N,
                                        tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    libbitcoin::data_chunk seed(PrivateSeed.p, PrivateSeed.end);
    libwallet::hd_private_key m(seed);
    libwallet::hd_private_key m0 = m.generate_private_key(0);
    libwallet::hd_private_key m00 = m0.generate_private_key(0);
    libwallet::hd_private_key m00n = m00.generate_private_key(N);
    if (m00n.valid())
    {
        std::string out = bc::encode_hex(m00n.private_key());
        ABC_STRDUP(*pszPrivAddress, out.c_str());
    }
    else
    {
        *pszPrivAddress = nullptr;
    }

exit:
    return cc;
}

tABC_CC ABC_BridgeWatcherStart(const char *szUserName,
                               const char *szPassword,
                               const char *szWalletUUID,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_AccountGeneralInfo *ppInfo = NULL;

    char *szUserCopy;
    char *szPassCopy;
    char *szUUIDCopy;

    WatcherInfo *watcherInfo = new WatcherInfo();
    watcherInfo->watcher = new libwallet::watcher();

    ABC_STRDUP(szUserCopy, szUserName);
    ABC_STRDUP(szPassCopy, szPassword);
    ABC_STRDUP(szUUIDCopy, szWalletUUID);

    watcherInfo->szUserName = szUserCopy;
    watcherInfo->szPassword = szPassCopy;
    watcherInfo->szWalletUUID = szUUIDCopy;

    watcherInfo->callback = [watcherInfo] (const libbitcoin::transaction_type& tx)
    {
        ABC_BridgeTxCallback(watcherInfo, tx);
    };

    ABC_CHECK_RET(ABC_AccountLoadGeneralInfo(&ppInfo, pError));
    /* Set transaction callback */
    ABC_DebugLog("setting callback\n");
    watcherInfo->watcher->set_callback(watcherInfo->callback);

    if (false && ppInfo->countObeliskServers > 0)
    {
        ABC_DebugLog("Using %s obelisk servers\n",
                ppInfo->aszObeliskServers[0]);
        watcherInfo->watcher->connect(ppInfo->aszObeliskServers[0]);
    }
    else
    {
        ABC_DebugLog("Using Fallback obelisk servers: %s\n", FALLBACK_OBELISK);
        watcherInfo->watcher->connect(FALLBACK_OBELISK);
    }
    ABC_BridgeWatcherLoad(watcherInfo, pError);
    watchers_[szWalletUUID] = watcherInfo;
exit:
    ABC_AccountFreeGeneralInfo(ppInfo);
    return cc;
}

tABC_CC ABC_BridgeWatchAddr(const char *szUserName,
                            const char *szPassword,
                            const char *szWalletUUID,
                            const char *pubAddress,
                            bool prioritize,
                            tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    auto row = watchers_.find(szWalletUUID);

    ABC_DebugLog("Watching %s for %s\n", pubAddress, szWalletUUID);
    bc::payment_address addr;

    ABC_CHECK_ASSERT(row != watchers_.end(),
        ABC_CC_Error, "Unable find watcher");

    if (!addr.set_encoded(pubAddress))
    {
        cc = ABC_CC_Error;
        ABC_DebugLog("Invalid pubAddress %s\n", pubAddress);
        goto exit;
    }
    row->second->addresses.insert(pubAddress);
    row->second->watcher->watch_address(addr);
    if (prioritize)
    {
        row->second->watcher->prioritize_address(addr);
    }
exit:
    return cc;
}

tABC_CC ABC_BridgeWatcherStop(const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    auto row = watchers_.find(szWalletUUID);
    ABC_CHECK_ASSERT(row != watchers_.end(),
        ABC_CC_Error, "Unable find watcher");

    ABC_BridgeWatcherSerialize(row->second);

    row->second->watcher->disconnect();
    if (row->second->watcher != NULL) {
        delete row->second->watcher;
    }
    row->second->watcher = NULL;
    ABC_FREE_STR(row->second->szUserName);
    ABC_FREE_STR(row->second->szPassword);
    ABC_FREE_STR(row->second->szWalletUUID);
    if (row->second != NULL) {
        delete row->second;
    }
exit:
    return cc;
}

tABC_CC ABC_BridgeTxMake(tABC_TxSendInfo *pSendInfo,
                         char **addresses, int addressCount,
                         char *changeAddress,
                         tABC_UnsignedTx *pUtx,
                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    bc::payment_address change, ab, dest;
    libwallet::fee_schedule schedule;
    libwallet::unsigned_transaction_type *utx;
    bc::transaction_output_list outputs;
    uint64_t totalAmountSatoshi = 0, abFees = 0, minerFees = 0;
    tABC_AccountGeneralInfo *ppInfo = NULL;
    std::vector<bc::payment_address> addresses_;

    // Find a watcher to use
    auto row = watchers_.find(pSendInfo->szWalletUUID);
    ABC_CHECK_ASSERT(row != watchers_.end(),
        ABC_CC_Error, "Unable find watcher");

    // Alloc a new utx
    utx = new libwallet::unsigned_transaction_type();
    ABC_CHECK_ASSERT(utx != NULL,
        ABC_CC_NULLPtr, "Unable alloc unsigned_transaction_type");

    // Fetch Info to calculate fees
    ABC_CHECK_RET(ABC_AccountLoadGeneralInfo(&ppInfo, pError));
    // Create payment_addresses
    ABC_CHECK_ASSERT(addressCount > 0,
        ABC_CC_Error, "No addresses supplied");
    for (int i = 0; i < addressCount; ++i)
    {
        bc::payment_address pa;
        ABC_CHECK_ASSERT(true == pa.set_encoded(addresses[i]),
            ABC_CC_Error, "Bad source address");
        std::cout << "Including in outputs: " <<  pa.encoded() << std::endl;
        addresses_.push_back(pa);
    }
    ABC_CHECK_ASSERT(true == change.set_encoded(changeAddress),
        ABC_CC_Error, "Bad change address");
    ABC_CHECK_ASSERT(true == dest.set_encoded(pSendInfo->szDestAddress),
        ABC_CC_Error, "Bad destination address");
    ABC_CHECK_ASSERT(true == ab.set_encoded(ppInfo->pAirBitzFee->szAddresss),
        ABC_CC_Error, "Bad ABV address");

    schedule.satoshi_per_kb = ppInfo->countMinersFees;
    totalAmountSatoshi = pSendInfo->pDetails->amountSatoshi;

    // Calculate AB Fees
    abFees = ABC_BridgeCalcAbFees(pSendInfo->pDetails->amountSatoshi, ppInfo);
    // Add in miners fees
    if (abFees > 0)
    {
        pSendInfo->pDetails->amountFeesAirbitzSatoshi = abFees;
        // Output to Airbitz
        ABC_BridgeAppendOutput(outputs, abFees, ab.hash());
        // Increment total tx amount to account for AB fees
        totalAmountSatoshi += abFees;
    }
    // Output to  Destination Address
    ABC_BridgeAppendOutput(outputs, pSendInfo->pDetails->amountSatoshi, dest.hash());

    minerFees = ABC_BridgeCalcMinerFees(utx, ppInfo);
    if (minerFees > 0)
    {
        // If there are miner fees, increase totalSatoshi
        pSendInfo->pDetails->amountFeesMinersSatoshi = minerFees;
        totalAmountSatoshi += minerFees;
    }
    // Set the fees in the send details
    pSendInfo->pDetails->amountFeesAirbitzSatoshi = abFees;
    pSendInfo->pDetails->amountFeesMinersSatoshi = minerFees;
    ABC_DebugLog("Change: %s, Amount: %ld, Amount w/Fees %d\n",
                    change.encoded().c_str(),
                    pSendInfo->pDetails->amountSatoshi,
                    totalAmountSatoshi);
    if (!libwallet::make_tx(*(row->second->watcher), addresses_, change,
                            totalAmountSatoshi, schedule, outputs, *utx))
    {
        ABC_CHECK_RET(ABC_BridgeTxErrorHandler(utx, pError));
    }

    pUtx->data = (void *) utx;
    pUtx->fees = minerFees;
exit:
    ABC_AccountFreeGeneralInfo(ppInfo);
    return cc;
}

tABC_CC ABC_BridgeTxSignSend(tABC_TxSendInfo *pSendInfo,
                             char **paPrivKey,
                             unsigned int keyCount,
                             tABC_UnsignedTx *pUtx,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    std::string txid;
    std::vector<bc::elliptic_curve_key> keys;
    libwallet::unsigned_transaction_type *utx;

    utx = (libwallet::unsigned_transaction_type *) pUtx->data;
    auto row = watchers_.find(pSendInfo->szWalletUUID);
    ABC_CHECK_ASSERT(row != watchers_.end(),
        ABC_CC_Error, "Unable find watcher");

    for (int i = 0; i < keyCount; ++i)
    {
        bc::elliptic_curve_key k;
        ABC_CHECK_RET(ABC_BridgeStringToEc(paPrivKey[i], k, pError));
        keys.push_back(k);
    }

    if (!libwallet::sign_send_tx(*(row->second->watcher), *utx, keys))
        ABC_CHECK_RET(ABC_BridgeTxErrorHandler(utx, pError));

    txid = ABC_BridgeNonMalleableTxId(utx->tx);
    ABC_STRDUP(pUtx->szTxId, txid.c_str());

    if (ABC_BridgeBlockhainPostTx(utx, pError) != ABC_CC_Ok)
    {
        ABC_RET_ERROR(ABC_CC_ServerError, pError->szDescription);
        ABC_DebugLog(pError->szDescription);
    }
exit:
    return cc;
}

static
tABC_CC ABC_BridgeTxErrorHandler(libwallet::unsigned_transaction_type *utx, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    ABC_DebugLog("Transaction Error Code: %d\n", utx->code);
    switch (utx->code)
    {
        case libwallet::insufficent_funds:
            ABC_RET_ERROR(ABC_CC_InsufficientFunds, "Insufficent funds.");
        case libwallet::invalid_key:
            ABC_RET_ERROR(ABC_CC_Error, "Invalid address.");
        case libwallet::invalid_sig:
            ABC_RET_ERROR(ABC_CC_Error, "Unable to sign.");
        default:
            break;
    }
exit:
    return cc;
}

static
void ABC_BridgeTxCallback(WatcherInfo *watcherInfo, const libbitcoin::transaction_type& tx)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Error error;
    int64_t fees = 0;
    int64_t totalInSatoshi = 0, totalOutSatoshi = 0, totalMeSatoshi = 0, totalMeInSatoshi = 0;
    char **iarr = NULL, **oarr = NULL;
    std::vector<std::string> oaddresses, iaddresses;
    std::string txId, malTxId;

    if (watcherInfo == NULL)
    {
        cc = ABC_CC_Error;
        goto exit;
    }
    ABC_DebugLog("ABC_BridgeTxCallback %s %s %s\n",
        watcherInfo->szUserName, watcherInfo->szPassword, watcherInfo->szWalletUUID);

    for (auto i : tx.inputs)
    {
        bc::payment_address addr;
        bc::extract(addr, i.script);

        auto prev = i.previous_output;
        auto tx = watcherInfo->watcher->find_tx(prev.hash);
        if (prev.index < tx.outputs.size())
        {
            totalInSatoshi += tx.outputs[prev.index].value;
            auto row = watcherInfo->addresses.find(addr.encoded());
            if  (row != watcherInfo->addresses.end())
                totalMeInSatoshi += tx.outputs[prev.index].value;
        }
        iaddresses.push_back(addr.encoded());
    }

    for (auto o : tx.outputs)
    {
        bc::payment_address addr;
        bc::extract(addr, o.script);
        auto row = watcherInfo->addresses.find(addr.encoded());
        if  (row != watcherInfo->addresses.end())
        {
            oaddresses.push_back(addr.encoded());
            totalMeSatoshi += o.value;
            // TODO: load address and mark recycleable = false
        }
        totalOutSatoshi += o.value;
    }
    if (totalMeSatoshi == 0 && totalMeInSatoshi == 0)
    {
        ABC_DebugLog("values == 0, this tx does not concern me.\n");
        goto exit;
    }
    fees = totalInSatoshi - totalOutSatoshi;
    totalMeSatoshi -= totalMeInSatoshi;

    ABC_BridgeVectorCopy(&iarr, iaddresses);
    ABC_BridgeVectorCopy(&oarr, oaddresses);

    ABC_DebugLog("calling ABC_TxReceiveTransaction\n");
    ABC_DebugLog("Total Me: %d, Total In: %d, Total Out: %d, Fees: %d\n",
                    totalMeSatoshi, totalInSatoshi, totalOutSatoshi, fees);
    txId = ABC_BridgeNonMalleableTxId(tx);
    malTxId = bc::encode_hex(bc::hash_transaction(tx));
    ABC_DebugLog("Non-Malleable: %s\n", txId.c_str());
    ABC_DebugLog("Malleable: %s\n", malTxId.c_str());
    ABC_CHECK_RET(
        ABC_TxReceiveTransaction(
            watcherInfo->szUserName, watcherInfo->szPassword, watcherInfo->szWalletUUID,
            totalMeSatoshi, fees,
            iarr, iaddresses.size(),
            oarr, oaddresses.size(),
            txId.c_str(), malTxId.c_str(), &error));
    ABC_BridgeWatcherSerializeAsync(watcherInfo);
exit:
    ABC_UtilFreeStringArray(oarr, oaddresses.size());
    ABC_UtilFreeStringArray(iarr, iaddresses.size());
}

static
tABC_CC ABC_BridgeStringToEc(char *privKey, bc::elliptic_curve_key& key, tABC_Error *pError)
{
    bool compressed = true;
    tABC_CC cc = ABC_CC_Ok;
    bc::secret_parameter secret;

    secret = bc::decode_hex_digest<bc::hash_digest>(privKey);
    if (secret == bc::null_hash)
    {
        secret = libwallet::wif_to_secret(privKey);
        compressed = libwallet::is_wif_compressed(privKey);
    }
    ABC_CHECK_ASSERT(key.set_secret(secret, compressed) == true,
            ABC_CC_Error, "Unable to create elliptic_curve_key");
exit:
    return cc;
}

static
void ABC_BridgeAppendOutput(bc::transaction_output_list& outputs, uint64_t amount, const bc::short_hash &addr)
{
    bc::transaction_output_type output;
    output.value = amount;
    output.script.push_operation({bc::opcode::dup, bc::data_chunk()});
    output.script.push_operation({bc::opcode::hash160, bc::data_chunk()});
    output.script.push_operation({bc::opcode::special,
        bc::data_chunk(addr.begin(), addr.end())});
    output.script.push_operation({bc::opcode::equalverify, bc::data_chunk()});
    output.script.push_operation({bc::opcode::checksig, bc::data_chunk()});
    outputs.push_back(output);
}

static
void ABC_BridgeVectorCopy(char ***arr, std::vector<std::string> addresses)
{
    char **narr = new char * [addresses.size()];
    for (size_t i = 0; i < addresses.size(); i++)
    {
        narr[i] = new char[addresses[i].size() + 1];
        strcpy(narr[i], addresses[i].c_str());
    }
    *arr = narr;
}

static
uint64_t ABC_BridgeCalcAbFees(uint64_t amount,
                      tABC_AccountGeneralInfo *pInfo)
{
    uint64_t abFees =
        (uint64_t) ((double) amount *
                    (pInfo->pAirBitzFee->percentage * 0.01));
    ABC_DebugLog("Percent satoshi: %ld\n", abFees);
    abFees = AB_MIN(pInfo->pAirBitzFee->minSatoshi,
                AB_MAX(pInfo->pAirBitzFee->maxSatoshi, abFees));
    ABC_DebugLog("Bounded fees satoshi: %ld\n", abFees);
    return abFees;
}

static
uint64_t ABC_BridgeCalcMinerFees(libwallet::unsigned_transaction_type *utx,
                         tABC_AccountGeneralInfo *pInfo)
{
    uint64_t fees = 0;
    size_t tx_size = bc::satoshi_raw_size(utx->tx);
    ABC_DebugLog("Tx Size: %ld\n", tx_size);
    if (pInfo->countMinersFees > 0)
    {
        for (int i = 0; i < pInfo->countMinersFees; ++i)
        {
            if (tx_size <= pInfo->aMinersFees[i]->sizeTransaction)
            {
                fees = pInfo->aMinersFees[i]->amountSatoshi;
                break;
            }
        }
    }
    ABC_DebugLog("Miner Fees: %ld\n", fees);
    return fees;
}

static
std::string ABC_BridgeWatcherFile(const char *szUserName, const char *szPassword, const char *szWalletUUID)
{
    char *szDirName = NULL;
    tABC_Error error;
    ABC_AccountGetDirName(szUserName, &szDirName, &error);

    std::string filepath;
    filepath.append(std::string(szDirName));
    filepath.append("/w_");
    filepath.append(szWalletUUID);
    filepath.append(".ser");
    return filepath;
}

static
tABC_CC ABC_BridgeWatcherLoad(WatcherInfo *watcherInfo, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    uint8_t *pData = NULL;
    std::streampos size;

    std::string filepath(
            ABC_BridgeWatcherFile(watcherInfo->szUserName,
                                  watcherInfo->szPassword,
                                  watcherInfo->szWalletUUID));
    std::ifstream file(filepath, std::ios::in | std::ios::binary | std::ios::ate);
    ABC_CHECK_ASSERT(file.is_open() == true, ABC_CC_Error, "Unable to open file for loading");

    size = file.tellg();
    pData = new uint8_t[size];
    file.seekg(0, std::ios::beg);
    file.read(reinterpret_cast<char *>(pData), size);
    file.close();

    ABC_CHECK_ASSERT(watcherInfo->watcher->load(bc::data_chunk(pData, pData + size)) == true,
        ABC_CC_Error, "Unable to load serialized state\n");
exit:
    ABC_FREE(pData);
    return cc;
}

static
void ABC_BridgeWatcherSerializeAsync(WatcherInfo *watcherInfo)
{
    pthread_t handle;
    if (!pthread_create(&handle, NULL, ABC_BridgeWatcherSerialize, watcherInfo))
    {
        pthread_detach(handle);
    }
}

static
void *ABC_BridgeWatcherSerialize(void *pData)
{
    bc::data_chunk db;
    WatcherInfo *watcherInfo = (WatcherInfo *) pData;
    std::string filepath(
            ABC_BridgeWatcherFile(watcherInfo->szUserName,
                                  watcherInfo->szPassword,
                                  watcherInfo->szWalletUUID));

    std::ofstream file(filepath);
    if (!file.is_open())
    {
        ABC_DebugLog("Unable to open file for serialization");
    }
    else
    {
        db = watcherInfo->watcher->serialize();
        file.write(reinterpret_cast<const char *>(db.data()), db.size());
        file.close();
    }
}

static
tABC_CC ABC_BridgeBlockhainPostTx(libwallet::unsigned_transaction_type *utx, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    CURL *pCurlHandle = NULL;
    CURLcode curlCode;
    long resCode;
    std::string url, body, resBuffer;

    bc::data_chunk raw_tx(satoshi_raw_size(utx->tx));
    bc::satoshi_save(utx->tx, raw_tx.begin());
    std::string encoded(bc::encode_hex(raw_tx));
    std::string pretty(bc::pretty(utx->tx));

    url.append("http://blockchain.info/pushtx");
    body.append("tx=");
    body.append(encoded);

    ABC_DebugLog("%s\n", body.c_str());
    ABC_DebugLog("\n");
    ABC_DebugLog("%s\n", pretty.c_str());

    pCurlHandle = curl_easy_init();
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, url.c_str())) == 0,
            ABC_CC_Error, "Curl failed to set URL\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_POSTFIELDS, body.c_str())) == 0,
        ABC_CC_Error, "Curl failed to set post fields\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEDATA, &resBuffer)) == 0,
        ABC_CC_Error, "Curl failed to set data\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_WRITEFUNCTION, ABC_BridgeCurlWriteData)) == 0,
        ABC_CC_Error, "Curl failed to set callback\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_perform(pCurlHandle)) == 0,
        ABC_CC_Error, "Curl failed to perform\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_getinfo(pCurlHandle, CURLINFO_RESPONSE_CODE, &resCode)) == 0,
        ABC_CC_Error, "Curl failed to retrieve response info\n");

    ABC_DebugLog("%s\n", resBuffer.c_str());
    ABC_CHECK_ASSERT(resCode == 200, ABC_CC_Error, resBuffer.c_str());
exit:
    if (pCurlHandle != NULL)
        curl_easy_cleanup(pCurlHandle);

    return cc;
}

static
size_t ABC_BridgeCurlWriteData(void *pBuffer, size_t memberSize, size_t numMembers, void *pUserData)
{
    std::string *pCurlBuffer = (std::string *) pUserData;
    unsigned int dataAvailLength = (unsigned int) numMembers * (unsigned int) memberSize;
    size_t amountWritten = 0;

    if (pCurlBuffer)
    {
        pCurlBuffer->append((char *) pBuffer);
        amountWritten = dataAvailLength;
    }

    return amountWritten;
}

/**
 * Create a malleable tx id
 *
 * @param tx    The libbitcoin transaction_type which will be used to create a malleable txid
 */
static std::string ABC_BridgeNonMalleableTxId(const bc::transaction_type& tx)
{
    bc::data_chunk chunk;
    std::string txid;
    for (auto a : tx.inputs)
        for (auto b : bc::save_script(a.script))
            chunk.push_back(b);
    return bc::encode_hex(bc::generate_sha256_hash(chunk));
}

