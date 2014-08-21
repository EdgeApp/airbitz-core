/**
 * @file
 * AirBitz C++ wrappers.
 *
 * This file contains a bridge between the plain C code in the core, and
 * the C++ code in libbitcoin and friends.
 *
 *  Copyright (c) 2014, Airbitz
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms are permitted provided that
 *  the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *  3. Redistribution or use of modified source code requires the express written
 *  permission of Airbitz Inc.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 *  ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are those
 *  of the authors and should not be interpreted as representing official policies,
 *  either expressed or implied, of the Airbitz Project.
 *
 *  @author See AUTHORS
 *  @version 1.0
 */

#include "ABC_Bridge.h"
#include "ABC_General.h"
#include "ABC_Wallet.h"
#include "ABC_URL.h"
#include <curl/curl.h>
#include <wallet/wallet.hpp>
#include <unordered_map>
#include <bitcoin/watcher.hpp> // Includes the rest of the stack

#include "config.h"

#define FALLBACK_OBELISK "tcp://obelisk3.airbitz.co:9091"
#define TESTNET_OBELISK "tcp://obelisk-testnet2.airbitz.co:9091"
#define NO_AB_FEES

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
    std::set<std::string> addresses;
    char *szWalletUUID;
    char *szUserName;
    char *szPassword;
};

static uint8_t pubkey_version = 0x00;
static uint8_t script_version = 0x05;
typedef std::string WalletUUID;
static std::map<WalletUUID, WatcherInfo*> watchers_;

// XXX: Hacky. These hold the state on a send which is used on the return
// callback. It would be better to pass these into the watcher.
static tABC_TxSendInfo *gSendInfo = NULL;
static tABC_UnsignedTx *gUtx = NULL;

#if !NETWORK_FAKE
static void        ABC_BridgeTxCallback(WatcherInfo *watcherInfo, const libbitcoin::transaction_type& tx, tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback, void *pData);
static void        ABC_BridgeSendTxCallback(WatcherInfo *watcherInfo, const std::error_code &e, const libbitcoin::transaction_type& tx, tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback, void *pData);
static tABC_CC     ABC_BridgeExtractOutputs(libwallet::watcher *watcher, libwallet::unsigned_transaction_type *utx, std::string malleableId, tABC_UnsignedTx *pUtx, tABC_Error *pError);
static tABC_CC     ABC_BridgeTxErrorHandler(libwallet::unsigned_transaction_type *utx, tABC_Error *pError);
static void        ABC_BridgeAppendOutput(bc::transaction_output_list& outputs, uint64_t amount, const bc::payment_address &addr);
static bc::script_type ABC_BridgeCreateScriptHash(const bc::short_hash &script_hash);
static bc::script_type ABC_BridgeCreatePubKeyHash(const bc::short_hash &pubkey_hash);
static tABC_CC     ABC_BridgeStringToEc(char *privKey, bc::elliptic_curve_key& key, tABC_Error *pError);
static uint64_t    ABC_BridgeCalcAbFees(uint64_t amount, tABC_GeneralInfo *pInfo);
static uint64_t    ABC_BridgeCalcMinerFees(size_t tx_size, tABC_GeneralInfo *pInfo);
static std::string ABC_BridgeWatcherFile(const char *szUserName, const char *szPassword, const char *szWalletUUID);
static tABC_CC     ABC_BridgeWatcherLoad(WatcherInfo *watcherInfo, tABC_Error *pError);
static void        ABC_BridgeWatcherSerializeAsync(WatcherInfo *watcherInfo);
static void        *ABC_BridgeWatcherSerialize(void *pData);
static std::string ABC_BridgeNonMalleableTxId(const bc::transaction_type& tx);

static tABC_CC     ABC_BridgeChainPostTx(libwallet::unsigned_transaction_type *utx, tABC_Error *pError);
static tABC_CC     ABC_BridgeBlockhainPostTx(libwallet::unsigned_transaction_type *utx, tABC_Error *pError);
static size_t      ABC_BridgeCurlWriteData(void *pBuffer, size_t memberSize, size_t numMembers, void *pUserData);
#endif

/**
 * Prepares the event subsystem for operation.
 */
tABC_CC ABC_BridgeInitialize(tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;

    if (ABC_BridgeIsTestNet())
    {
        pubkey_version = 0x6f;
        script_version = 0xc4;
    }

    return cc;
}

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
    libbitcoin::payment_address address;
    char *uriString = NULL;
    tABC_CC cc = ABC_CC_Ok;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);

    tABC_BitcoinURIInfo *pInfo = NULL;
    std::string tempStr = szURI;

    ABC_CHECK_NULL(szURI);
    ABC_CHECK_ASSERT(strlen(szURI) > 0, ABC_CC_Error, "No URI provided");
    ABC_CHECK_NULL(ppInfo);
    *ppInfo = NULL;

    // allocate initial struct
    ABC_ALLOC_ARRAY(pInfo, 1, tABC_BitcoinURIInfo);

    // XX semi-hack warning. Might not be BIP friendly. Convert "bitcoin://1zf7ef..." URIs to
    // "bitcoin:1zf7ef..." so that libwallet parser doesn't choke. "bitcoin://" URLs are the
    // only style that are understood by email and SMS readers and will get forwarded
    // to bitcoin wallets. Airbitz wallet creates "bitcoin://" URIs when requesting funds via
    // email/SMS so it should be able to parse them as well. -paulvp

    ABC_STRDUP(uriString, szURI);

    if (0 == tempStr.find("bitcoin://", 0))
    {
        tempStr.replace(0, 10, "bitcoin:");
        std::size_t length = tempStr.copy(uriString,0xFFFFFFFF);
        uriString[length] = '\0';
    }

    // parse and extract contents
    if (!libwallet::uri_parse(uriString, result))
    {
        if (!address.set_encoded(uriString))
            ABC_RET_ERROR(ABC_CC_ParseError, "Malformed bitcoin URI");
        result.address = address;
    }
    if (result.address)
        ABC_STRDUP(pInfo->szAddress, result.address.get().encoded().c_str());
    if (result.amount)
        pInfo->amountSatoshi = result.amount.get();
    if (result.label)
        ABC_STRDUP(pInfo->szLabel, result.label.get().c_str());
    if (result.message)
        ABC_STRDUP(pInfo->szMessage, result.message.get().c_str());

    // Reject altcoin addresses:
    if (result.address.get().version() != pubkey_version &&
        result.address.get().version() != script_version)
    {
        ABC_RET_ERROR(ABC_CC_ParseError, "Wrong network URI");
    }

    // assign created info struct
    *ppInfo = pInfo;
    pInfo = NULL; // do this so we don't free below what we just gave the caller

exit:
    ABC_BridgeFreeURIInfo(pInfo);
    ABC_FREE(uriString);

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
                              uint64_t *pAmountOut,
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
tABC_CC ABC_BridgeFormatAmount(uint64_t amount,
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

#if !NETWORK_FAKE
    tABC_GeneralInfo *ppInfo = NULL;
    WatcherInfo *watcherInfo = NULL;
    char *szUserCopy;
    char *szPassCopy;
    char *szUUIDCopy;

    auto row = watchers_.find(szWalletUUID);
    if (row != watchers_.end()) {
        ABC_DebugLog("Watcher %s already initialized\n", szWalletUUID);
        goto exit;
    }

    watcherInfo = new WatcherInfo();
    watcherInfo->watcher = new libwallet::watcher();

    ABC_STRDUP(szUserCopy, szUserName);
    ABC_STRDUP(szPassCopy, szPassword);
    ABC_STRDUP(szUUIDCopy, szWalletUUID);

    watcherInfo->szUserName = szUserCopy;
    watcherInfo->szPassword = szPassCopy;
    watcherInfo->szWalletUUID = szUUIDCopy;

    ABC_CHECK_RET(ABC_GeneralGetInfo(&ppInfo, pError));

    if (false && ppInfo->countObeliskServers > 0)
    {
        ABC_DebugLog("Using %s obelisk servers\n",
                ppInfo->aszObeliskServers[0]);
        watcherInfo->watcher->connect(ppInfo->aszObeliskServers[0]);
    }
    else
    {
        if (ABC_BridgeIsTestNet())
        {
            ABC_DebugLog("Using Fallback testnet obelisk servers: %s\n", TESTNET_OBELISK);
            watcherInfo->watcher->connect(TESTNET_OBELISK);
        }
        else
        {
            ABC_DebugLog("Using Fallback obelisk servers: %s\n", FALLBACK_OBELISK);
            watcherInfo->watcher->connect(FALLBACK_OBELISK);
        }
    }
    ABC_BridgeWatcherLoad(watcherInfo, pError);
    watchers_[szWalletUUID] = watcherInfo;
exit:
    ABC_GeneralFreeInfo(ppInfo);
#endif // NETWORK_FAKE
    return cc;
}

tABC_CC ABC_BridgeWatcherLoop(const char *szWalletUUID, 
                              tABC_BitCoin_Event_Callback fAsyncCallback,
                              void *pData,
                              tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
#if !NETWORK_FAKE
    WatcherInfo *watcherInfo = NULL;
    libwallet::watcher::block_height_callback heightCallback;
    libwallet::watcher::callback txCallback;
    libwallet::watcher::tx_sent_callback sendCallback;

    auto row = watchers_.find(szWalletUUID);
    if (row == watchers_.end())
    {
        ABC_DebugLog("Watcher %s does not exist\n", szWalletUUID);
        goto exit;
    }

    watcherInfo = row->second;
    txCallback = [watcherInfo, fAsyncCallback, pData] (const libbitcoin::transaction_type& tx)
    {
        ABC_BridgeTxCallback(watcherInfo, tx, fAsyncCallback, pData);
    };
    watcherInfo->watcher->set_callback(txCallback);

    heightCallback = [watcherInfo, fAsyncCallback, pData](const size_t height)
    {
        tABC_Error error;
        ABC_TxBlockHeightUpdate(height, fAsyncCallback, pData, &error);
        ABC_BridgeWatcherSerializeAsync(watcherInfo);
    };
    watcherInfo->watcher->set_height_callback(heightCallback);

    sendCallback = [watcherInfo, fAsyncCallback, pData](const std::error_code &e, const::libbitcoin::transaction_type &tx)
    {
        ABC_BridgeSendTxCallback(watcherInfo, e, tx, fAsyncCallback, pData);
    };
    watcherInfo->watcher->set_tx_sent_callback(sendCallback);

    row->second->watcher->loop();
exit:
#endif // NETWORK_FAKE
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
#if !NETWORK_FAKE
    auto row = watchers_.find(szWalletUUID);

    ABC_DebugLog("Watching %s for %s\n", pubAddress, szWalletUUID);
    bc::payment_address addr;

    if (row == watchers_.end())
    {
        goto exit;
    }

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
#endif // NETWORK_FAKE
    return cc;
}

tABC_CC ABC_BridgeWatcherStop(const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
#if !NETWORK_FAKE
    auto row = watchers_.find(szWalletUUID);
    if (row == watchers_.end())
    {
        ABC_DebugLog("Watcher %s does not exist\n", szWalletUUID);
        goto exit;
    }
    row->second->watcher->stop();
exit:
#endif // NETWORK_FAKE
    return cc;
}

tABC_CC ABC_BridgeWatcherDelete(const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
#if !NETWORK_FAKE

    WatcherInfo *watcherInfo = NULL;

    auto row = watchers_.find(szWalletUUID);
    if (row == watchers_.end())
    {
        ABC_DebugLog("Watcher %s does not exist\n", szWalletUUID);
        goto exit;
    }
    watcherInfo = row->second;

    // Remove info from map:
    watchers_.erase(szWalletUUID);

    // Delete watcher:
    ABC_BridgeWatcherSerialize(watcherInfo);
    if (watcherInfo->watcher != NULL) {
        watcherInfo->watcher->disconnect();
        delete watcherInfo->watcher;
    }
    watcherInfo->watcher = NULL;

    // Delete info:
    ABC_FREE_STR(watcherInfo->szUserName);
    ABC_FREE_STR(watcherInfo->szPassword);
    ABC_FREE_STR(watcherInfo->szWalletUUID);
    if (watcherInfo != NULL) {
        delete watcherInfo;
    }

exit:
#endif // NETWORK_FAKE
    return cc;
}

tABC_CC ABC_BridgeTxMake(tABC_TxSendInfo *pSendInfo,
                         char **addresses, int addressCount,
                         char *changeAddress,
                         tABC_UnsignedTx *pUtx,
                         tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
#if !NETWORK_FAKE
    tABC_GeneralInfo *ppInfo = NULL;
    bc::payment_address change, ab, dest;
    libwallet::fee_schedule schedule;
    libwallet::unsigned_transaction_type *utx;
    bc::transaction_output_list outputs;
    uint64_t totalAmountSatoshi = 0, abFees = 0, minerFees = 0;
    std::vector<bc::payment_address> addresses_;

    // Find a watcher to use
    auto row = watchers_.find(pSendInfo->szWalletUUID);
    ABC_CHECK_ASSERT(row != watchers_.end(),
        ABC_CC_Error, "Unable find watcher");

    // Alloc a new utx
    utx = new libwallet::unsigned_transaction_type();
    ABC_CHECK_ASSERT(utx != NULL,
        ABC_CC_NULLPtr, "Unable alloc unsigned_transaction_type");

    // Update general info before send
    ABC_CHECK_RET(ABC_GeneralUpdateInfo(pError));
    // Fetch Info to calculate fees
    ABC_CHECK_RET(ABC_GeneralGetInfo(&ppInfo, pError));
    // Create payment_addresses
    ABC_CHECK_ASSERT(addressCount > 0,
        ABC_CC_Error, "No addresses supplied");
    for (int i = 0; i < addressCount; ++i)
    {
        bc::payment_address pa;
        ABC_CHECK_ASSERT(true == pa.set_encoded(addresses[i]),
            ABC_CC_Error, "Bad source address");
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

    if (!pSendInfo->bTransfer)
    {
        // Calculate AB Fees
        abFees = ABC_BridgeCalcAbFees(pSendInfo->pDetails->amountSatoshi, ppInfo);

        // Add in miners fees
        if (abFees > 0)
        {
            pSendInfo->pDetails->amountFeesAirbitzSatoshi = abFees;
            // Output to Airbitz
            ABC_BridgeAppendOutput(outputs, abFees, ab);
            // Increment total tx amount to account for AB fees
            totalAmountSatoshi += abFees;
        }
    }
    // Output to  Destination Address
    ABC_BridgeAppendOutput(outputs, pSendInfo->pDetails->amountSatoshi, dest);

    minerFees = ABC_BridgeCalcMinerFees(bc::satoshi_raw_size(utx->tx), ppInfo);
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
    ABC_GeneralFreeInfo(ppInfo);
#endif // NETWORK_FAKE
    return cc;
}

tABC_CC ABC_BridgeTxSignSend(tABC_TxSendInfo *pSendInfo,
                             char **paPrivKey,
                             unsigned int keyCount,
                             tABC_UnsignedTx *pUtx,
                             tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
#if !NETWORK_FAKE
    std::vector<bc::elliptic_curve_key> keys;
    libwallet::unsigned_transaction_type *utx;
    WatcherInfo *watcherInfo = NULL;

    utx = (libwallet::unsigned_transaction_type *) pUtx->data;
    auto row = watchers_.find(pSendInfo->szWalletUUID);
    ABC_CHECK_ASSERT(row != watchers_.end(),
        ABC_CC_Error, "Unable find watcher");

    watcherInfo = row->second;

    for (unsigned i = 0; i < keyCount; ++i)
    {
        bc::elliptic_curve_key k;
        ABC_CHECK_RET(ABC_BridgeStringToEc(paPrivKey[i], k, pError));
        keys.push_back(k);
    }
    // Set global state....ewww
    gSendInfo = pSendInfo;
    gUtx = pUtx;

    if (!libwallet::sign_send_tx(*(row->second->watcher), *utx, keys))
    {
        ABC_CHECK_RET(ABC_BridgeTxErrorHandler(utx, pError));
    }
exit:
#endif // NETWORK_FAKE
    return cc;
}

tABC_CC ABC_BridgeMaxSpendable(const char *szUserName,
                               const char *szPassword,
                               const char *szWalletUUID,
                               const char *szDestAddress,
                               bool bTransfer,
                               uint64_t *pMaxSatoshi,
                               tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
#if !NETWORK_FAKE
    tABC_TxSendInfo SendInfo;
    tABC_TxDetails Details;
    tABC_GeneralInfo *ppInfo = NULL;
    tABC_UnsignedTx utx;
    tABC_CC txResp;

    char *changeAddr = NULL;
    char **paAddresses = NULL;
    unsigned int countAddresses = 0;

    auto row = watchers_.find(szWalletUUID);
    uint64_t total = 0;

    ABC_CHECK_ASSERT(row != watchers_.end(),
        ABC_CC_Error, "Unable find watcher");

    ABC_STRDUP(SendInfo.szUserName, szUserName);
    ABC_STRDUP(SendInfo.szPassword, szPassword);
    ABC_STRDUP(SendInfo.szWalletUUID, szWalletUUID);
    ABC_STRDUP(SendInfo.szDestAddress, szDestAddress);

    // Snag the latest general info
    ABC_CHECK_RET(ABC_GeneralGetInfo(&ppInfo, pError));
    // Fetch all the payment addresses for this wallet
    ABC_CHECK_RET(
        ABC_TxGetPubAddresses(szUserName, szPassword, szWalletUUID,
                              &paAddresses, &countAddresses, pError));
    if (countAddresses > 0)
    {
        // This is needed to pass to the ABC_BridgeTxMake
        // It should never be used
        changeAddr = paAddresses[0];

        // Calculate total of utxos for these addresses
        ABC_DebugLog("Get UTOXs for %d\n", countAddresses);
        for (unsigned i = 0; i < countAddresses; ++i)
        {
            bc::payment_address pa;
            ABC_CHECK_ASSERT(true == pa.set_encoded(paAddresses[i]),
                ABC_CC_Error, "Bad source address");
            for (auto l : row->second->watcher->get_utxos(pa))
            {
                total += l.value;
            }
        }
        if (!bTransfer)
        {
            // Subtract ab tx fee
            total -= ABC_BridgeCalcAbFees(total, ppInfo);
        }
        // Subtract minimum tx fee
        total -= ABC_BridgeCalcMinerFees(0, ppInfo);

        SendInfo.pDetails = &Details;
        SendInfo.bTransfer = bTransfer;
        Details.amountSatoshi = total;

        // Ewwwwww, fix this to have minimal iterations
        txResp = ABC_BridgeTxMake(&SendInfo,
                                  paAddresses, countAddresses,
                                  changeAddr, &utx, pError);
        while (txResp == ABC_CC_InsufficientFunds && Details.amountSatoshi > 0)
        {
            Details.amountSatoshi -= 1;
            txResp = ABC_BridgeTxMake(&SendInfo,
                                      paAddresses, countAddresses,
                                      changeAddr, &utx, pError);
        }
        *pMaxSatoshi = AB_MAX(Details.amountSatoshi, 0);
    }
    else
    {
        *pMaxSatoshi = 0;
    }
exit:
    ABC_FREE_STR(SendInfo.szUserName);
    ABC_FREE_STR(SendInfo.szPassword);
    ABC_FREE_STR(SendInfo.szWalletUUID);
    ABC_FREE_STR(SendInfo.szDestAddress);
    ABC_GeneralFreeInfo(ppInfo);
    ABC_UtilFreeStringArray(paAddresses, countAddresses);
#endif
    return cc;
}

tABC_CC
ABC_BridgeTxHeight(const char *szWalletUUID, const char *szTxId, unsigned int *height, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
#if !NETWORK_FAKE
    int height_;
    bc::hash_digest txId;
    auto row = watchers_.find(szWalletUUID);
    if (row == watchers_.end())
    {
        cc = ABC_CC_Synchronizing;
        goto exit;
    }
    txId = bc::decode_hash(szTxId);
    if (!row->second->watcher->get_tx_height(txId, height_))
    {
        cc = ABC_CC_Synchronizing;
    }
    *height = height_;
exit:
#else
    *height = 0;
#endif
    return cc;
}

tABC_CC
ABC_BridgeTxBlockHeight(const char *szWalletUUID, unsigned int *height, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
#if !NETWORK_FAKE
    auto row = watchers_.find(szWalletUUID);
    if (row == watchers_.end())
    {
        cc = ABC_CC_Synchronizing;
        goto exit;
    }
    *height = row->second->watcher->get_last_block_height();
    if (*height == 0)
    {
        cc = ABC_CC_Synchronizing;
    }
exit:
#else
    *height = 0;
#endif
    return cc;
}

tABC_CC
ABC_BridgeWatcherStatus(const char *szWalletUUID, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
#if !NETWORK_FAKE
    auto row = watchers_.find(szWalletUUID);
    if (row == watchers_.end())
    {
        cc = ABC_CC_Synchronizing;
        goto exit;
    }

    if (row->second->watcher->get_status() == libwallet::watcher::watcher_syncing)
    {
        cc = ABC_CC_Synchronizing;
    }
exit:
#endif
    return cc;
}

bool
ABC_BridgeIsTestNet()
{
    bc::payment_address foo;
    bc::set_public_key_hash(foo, bc::null_short_hash);
    return foo.version() != 0;
}

#if !NETWORK_FAKE
static
void ABC_BridgeTxCallback(WatcherInfo *watcherInfo, const libbitcoin::transaction_type& tx,
                          tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                          void *pData)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Error error;
    tABC_Error *pError = &error;
    int64_t fees = 0;
    int64_t totalInSatoshi = 0, totalOutSatoshi = 0, totalMeSatoshi = 0, totalMeInSatoshi = 0;
    tABC_TxOutput **iarr = NULL, **oarr = NULL;
    unsigned int idx = 0, iCount = 0, oCount = 0;
    std::string txId, malTxId;

    if (watcherInfo == NULL)
    {
        cc = ABC_CC_Error;
        goto exit;
    }

    txId = ABC_BridgeNonMalleableTxId(tx);
    malTxId = bc::encode_hex(bc::hash_transaction(tx));

    idx = 0;
    iCount = tx.inputs.size();
    iarr = (tABC_TxOutput **) malloc(sizeof(tABC_TxOutput *) * iCount);
    for (auto i : tx.inputs)
    {
        bc::payment_address addr;
        bc::extract(addr, i.script);
        auto prev = i.previous_output;

        // Create output
        tABC_TxOutput *out = (tABC_TxOutput *) malloc(sizeof(tABC_TxOutput));
        out->input = true;
        ABC_STRDUP(out->szTxId, bc::encode_hex(prev.hash).c_str());
        ABC_STRDUP(out->szAddress, addr.encoded().c_str());

        // Check prevouts for values
        auto tx = watcherInfo->watcher->find_tx(prev.hash);
        if (prev.index < tx.outputs.size())
        {
            out->value = tx.outputs[prev.index].value;
            totalInSatoshi += tx.outputs[prev.index].value;
            auto row = watcherInfo->addresses.find(addr.encoded());
            if  (row != watcherInfo->addresses.end())
                totalMeInSatoshi += tx.outputs[prev.index].value;
        }
        iarr[idx] = out;
        idx++;
    }

    idx = 0;
    oCount = tx.outputs.size();
    oarr = (tABC_TxOutput **) malloc(sizeof(tABC_TxOutput *) * oCount);
    for (auto o : tx.outputs)
    {
        bc::payment_address addr;
        bc::extract(addr, o.script);
        // Create output
        tABC_TxOutput *out = (tABC_TxOutput *) malloc(sizeof(tABC_TxOutput));
        out->input = false;
        out->value = o.value;
        ABC_STRDUP(out->szAddress, addr.encoded().c_str());
        ABC_STRDUP(out->szTxId, malTxId.c_str());

        // Do we own this address?
        auto row = watcherInfo->addresses.find(addr.encoded());
        if  (row != watcherInfo->addresses.end())
        {
            totalMeSatoshi += o.value;
        }
        totalOutSatoshi += o.value;

        oarr[idx] = out;
        idx++;
    }
    if (totalMeSatoshi == 0 && totalMeInSatoshi == 0)
    {
        ABC_DebugLog("values == 0, this tx does not concern me.\n");
        goto exit;
    }
    fees = totalInSatoshi - totalOutSatoshi;
    totalMeSatoshi -= totalMeInSatoshi;

    ABC_DebugLog("calling ABC_TxReceiveTransaction\n");
    ABC_DebugLog("Total Me: %d, Total In: %d, Total Out: %d, Fees: %d\n",
                    totalMeSatoshi, totalInSatoshi, totalOutSatoshi, fees);
    ABC_CHECK_RET(
        ABC_TxReceiveTransaction(
            watcherInfo->szUserName, watcherInfo->szPassword, watcherInfo->szWalletUUID,
            totalMeSatoshi, fees,
            iarr, iCount,
            oarr, oCount,
            txId.c_str(), malTxId.c_str(), 
            fAsyncBitCoinEventCallback,
            pData,
            &error));
    ABC_BridgeWatcherSerializeAsync(watcherInfo);
exit:
    ABC_FREE(oarr);
    ABC_FREE(iarr);
}

static
void ABC_BridgeSendTxCallback(WatcherInfo *watcherInfo,
                              const std::error_code &e,
                              const libbitcoin::transaction_type& tx,
                              tABC_BitCoin_Event_Callback fAsyncBitCoinEventCallback,
                              void *pData)
{
    tABC_CC cc = ABC_CC_Ok;
    tABC_Error *pError = new tABC_Error;
    ABC_SET_ERR_CODE(pError, ABC_CC_Ok);
    std::string txid, malleableId;

    tABC_TxSendInfo *pSendInfo = gSendInfo;
    tABC_UnsignedTx *pUtx = gUtx;

    gSendInfo = NULL;
    gUtx = NULL;

    libwallet::unsigned_transaction_type *utx =
        (libwallet::unsigned_transaction_type *) pUtx->data;

    if (e.value())
    {
        pError->code = ABC_CC_Error;
        ABC_TxSendCompleteError(pSendInfo, pUtx, fAsyncBitCoinEventCallback, pData, pError);
    }
    else
    {
        ABC_BridgeChainPostTx(utx, pError);
        if (!ABC_BridgeIsTestNet())
        {
            ABC_BridgeBlockhainPostTx(utx, pError);
        }

        txid = ABC_BridgeNonMalleableTxId(utx->tx);
        ABC_STRDUP(pUtx->szTxId, txid.c_str());

        malleableId = bc::encode_hex(bc::hash_transaction(utx->tx));
        ABC_STRDUP(pUtx->szTxMalleableId, malleableId.c_str());

        ABC_BridgeWatcherSerializeAsync(watcherInfo);
        ABC_BridgeExtractOutputs(watcherInfo->watcher, utx, malleableId, pUtx,pError);

        ABC_TxSendComplete(pSendInfo, pUtx, fAsyncBitCoinEventCallback, pData, pError);
    }
exit:
    return;
}

static tABC_CC
ABC_BridgeExtractOutputs(libwallet::watcher *watcher, libwallet::unsigned_transaction_type *utx,
                         std::string malleableId, tABC_UnsignedTx *pUtx, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    pUtx->countOutputs = utx->tx.inputs.size() + utx->tx.outputs.size();
    pUtx->aOutputs = (tABC_TxOutput **) malloc(sizeof(tABC_TxOutput) * pUtx->countOutputs);
    int i = 0;
    for (auto& input : utx->tx.inputs)
    {
        auto prev = input.previous_output;
        bc::payment_address addr;
        bc::extract(addr, input.script);

        tABC_TxOutput *out = (tABC_TxOutput *) malloc(sizeof(tABC_TxOutput));
        out->input = true;
        ABC_STRDUP(out->szTxId, bc::encode_hex(prev.hash).c_str());
        ABC_STRDUP(out->szAddress, addr.encoded().c_str());

        auto tx = watcher->find_tx(prev.hash);
        if (prev.index < tx.outputs.size())
        {
            out->value = tx.outputs[prev.index].value;
        }
        pUtx->aOutputs[i] = out;
        i++;
    }
    for (auto& output : utx->tx.outputs)
    {
        bc::payment_address addr;
        bc::extract(addr, output.script);

        tABC_TxOutput *out = (tABC_TxOutput *) malloc(sizeof(tABC_TxOutput));
        out->input = false;
        out->value = output.value;
        ABC_STRDUP(out->szTxId, malleableId.c_str());
        ABC_STRDUP(out->szAddress, addr.encoded().c_str());

        pUtx->aOutputs[i] = out;
        i++;
    }
exit:
    return cc;
}

static
tABC_CC ABC_BridgeTxErrorHandler(libwallet::unsigned_transaction_type *utx, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
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
tABC_CC ABC_BridgeStringToEc(char *privKey, bc::elliptic_curve_key& key, tABC_Error *pError)
{
    bool compressed = true;
    tABC_CC cc = ABC_CC_Ok;
    bc::secret_parameter secret;

    secret = bc::decode_hash(privKey);
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
void ABC_BridgeAppendOutput(bc::transaction_output_list& outputs, uint64_t amount, const bc::payment_address &addr)
{
    bc::transaction_output_type output;
    output.value = amount;
    if (addr.version() == pubkey_version)
    {
        output.script = ABC_BridgeCreatePubKeyHash(addr.hash());
    }
    else if (addr.version() == script_version)
    {
        output.script = ABC_BridgeCreateScriptHash(addr.hash());
    }
    outputs.push_back(output);
}

static
bc::script_type ABC_BridgeCreateScriptHash(const bc::short_hash &script_hash)
{
    bc::script_type result;
    result.push_operation({bc::opcode::hash160, bc::data_chunk()});
    result.push_operation({bc::opcode::special, bc::data_chunk(script_hash.begin(), script_hash.end())});
    result.push_operation({bc::opcode::equal, bc::data_chunk()});
    return result;
}

static
bc::script_type ABC_BridgeCreatePubKeyHash(const bc::short_hash &pubkey_hash)
{
    bc::script_type result;
    result.push_operation({bc::opcode::dup, bc::data_chunk()});
    result.push_operation({bc::opcode::hash160, bc::data_chunk()});
    result.push_operation({bc::opcode::special,
        bc::data_chunk(pubkey_hash.begin(), pubkey_hash.end())});
    result.push_operation({bc::opcode::equalverify, bc::data_chunk()});
    result.push_operation({bc::opcode::checksig, bc::data_chunk()});
    return result;
}

static
uint64_t ABC_BridgeCalcAbFees(uint64_t amount, tABC_GeneralInfo *pInfo)
{

#ifdef NO_AB_FEES
    return 0;
#else
    uint64_t abFees =
        (uint64_t) ((double) amount *
                    (pInfo->pAirBitzFee->percentage * 0.01));
    abFees = AB_MAX(pInfo->pAirBitzFee->minSatoshi, abFees);
    abFees = AB_MIN(pInfo->pAirBitzFee->maxSatoshi, abFees);

    return abFees;
#endif
}

static
uint64_t ABC_BridgeCalcMinerFees(size_t tx_size, tABC_GeneralInfo *pInfo)
{
    uint64_t fees = 0;
    if (pInfo->countMinersFees > 0)
    {
        for (unsigned i = 0; i < pInfo->countMinersFees; ++i)
        {
            if (tx_size <= pInfo->aMinersFees[i]->sizeTransaction)
            {
                fees = pInfo->aMinersFees[i]->amountSatoshi;
                break;
            }
        }
    }
    return fees;
}

static
std::string ABC_BridgeWatcherFile(const char *szUserName, const char *szPassword, const char *szWalletUUID)
{
    char *szDirName = NULL;
    tABC_Error error;
    ABC_WalletGetDirName(&szDirName, szWalletUUID, &error);

    std::string filepath;
    filepath.append(std::string(szDirName));
    filepath.append("/watcher.ser");
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

    struct stat buffer;
    if (stat(filepath.c_str(), &buffer) == 0)
    {
        std::ifstream file(filepath, std::ios::in | std::ios::binary | std::ios::ate);
        ABC_CHECK_ASSERT(file.is_open() == true, ABC_CC_Error, "Unable to open file for loading");

        size = file.tellg();
        pData = new uint8_t[size];
        file.seekg(0, std::ios::beg);
        file.read(reinterpret_cast<char *>(pData), size);
        file.close();

        ABC_CHECK_ASSERT(watcherInfo->watcher->load(bc::data_chunk(pData, pData + size)) == true,
            ABC_CC_Error, "Unable to load serialized state\n");
    }
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
    return NULL;
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
    return bc::encode_hex(bc::sha256_hash(chunk));
}

static
tABC_CC ABC_BridgeChainPostTx(libwallet::unsigned_transaction_type *utx, tABC_Error *pError)
{
    tABC_CC cc = ABC_CC_Ok;
    CURL *pCurlHandle = NULL;
    CURLcode curlCode;
    long resCode;
    std::string url, resBuffer;
    json_t *pJSON_Root = NULL;
    char *szPut = NULL;

    bc::data_chunk raw_tx(satoshi_raw_size(utx->tx));
    bc::satoshi_save(utx->tx, raw_tx.begin());
    std::string encoded(bc::encode_hex(raw_tx));
    std::string pretty(bc::pretty(utx->tx));

    if (ABC_BridgeIsTestNet())
    {
        url.append("https://api.chain.com/v1/testnet3/transactions");
    }
    else
    {
        url.append("https://api.chain.com/v1/bitcoin/transactions");
    }

    pJSON_Root = json_pack("{ss}", "hex", encoded.c_str());
    szPut = ABC_UtilStringFromJSONObject(pJSON_Root, JSON_COMPACT);

    ABC_DebugLog("URL: %s\n", url.c_str());
    ABC_DebugLog("UserPwd: %s\n", CHAIN_API_USERPWD);
    ABC_DebugLog("Body: %s\n", szPut);
    ABC_DebugLog("\n");
    ABC_DebugLog("%s\n", pretty.c_str());

    ABC_CHECK_RET(ABC_URLCurlHandleInit(&pCurlHandle, pError))
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_URL, url.c_str())) == 0,
        ABC_CC_Error, "Curl failed to set URL\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_USERPWD, CHAIN_API_USERPWD)) == 0,
        ABC_CC_Error, "Curl failed to set User:Password\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_CUSTOMREQUEST, "PUT")) == 0,
        ABC_CC_Error, "Curl failed to set Put\n");
    ABC_CHECK_ASSERT((curlCode = curl_easy_setopt(pCurlHandle, CURLOPT_POSTFIELDS, szPut)) == 0,
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
    {
        curl_easy_cleanup(pCurlHandle);
    }
    json_decref(pJSON_Root);
    ABC_FREE_STR(szPut);
    
    return cc;
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

    url.append("https://blockchain.info/pushtx");
    body.append("tx=");
    body.append(encoded);

    ABC_DebugLog("%s\n", body.c_str());
    ABC_DebugLog("\n");
    ABC_DebugLog("%s\n", pretty.c_str());

    ABC_CHECK_RET(ABC_URLCurlHandleInit(&pCurlHandle, pError))
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


#endif // NETWORK_FAKE

